//********************************************************************************
// Author:    dfblackburn
// Created:   September 04, 2015
//********************************************************************************

#include <bur/plctypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <AxisErrLib.h>
#include <string.h>

#ifdef __cplusplus
};
#endif

//*******************************************************
// Add all current axis errors to an error collector
//*******************************************************

void AxisAddErrorsToCollector(struct AxisAddErrorsToCollector* t)
{

	// Check for invalid inputs
	if (t == 0) return;
	
	if ( (t->Axis == 0) || (t->pErrorCollector == 0) ){
		t->Error = 1;
		t->ErrorID = AXERR_ERR_INVALIDINPUT;
		strcpy(t->ErrorString, "Invalid Axis or ErrorCollector input");
		return;
	}
	
	// Set default error status
	t->Error = 0;
	t->ErrorID = 0;
	strcpy(t->ErrorString, "");
	
	if( !t->Enable ){
		
		t->internal.readAxisError.Enable = 0;
		MC_BR_ReadAxisError(&(t->internal.readAxisError));
		
		t->internal.getErrorText.Execute = 0;
		MC_BR_GetErrorText(&(t->internal.getErrorText));
		
		t->internal.writeParIDText.Execute = 0;
		MC_BR_WriteParIDText(&(t->internal.writeParIDText));
		
		t->internal.reset.Execute = 0;
		MC_Reset(&(t->internal.reset));
		
		t->internal.retrieveState = AXERR_RETRIEVE_ST_IDLE;
		t->internal.resetState = AXERR_RESET_ST_IDLE;
		
		return;
		
	}
	

	//*************************************
	// Retrieve axis errors
	//*************************************

	switch(t->internal.retrieveState) {
	
		case AXERR_RETRIEVE_ST_IDLE:
			
			t->internal.readAxisError.Acknowledge = 0;
			
			if (t->internal.readAxisError.ErrorRecordAvailable && t->internal.readAxisError.Valid) {
			
				t->internal.getErrorText.Configuration.Format = mcWRAP + mcNULL;
				t->internal.getErrorText.Configuration.LineLength = sizeof(t->internal.errorStringArray[0]);
				t->internal.getErrorText.Configuration.DataLength = sizeof(t->internal.errorStringArray);
				t->internal.getErrorText.Configuration.DataAddress = (UDINT)&(t->internal.errorStringArray);
				strcpy(t->internal.getErrorText.Configuration.DataObjectName, t->DataObjectName);

				t->internal.getErrorText.ErrorRecord = t->internal.readAxisError.ErrorRecord;//can i do this?

				t->internal.getErrorText.Execute = 1;
		
				t->internal.retrieveState = AXERR_RETRIEVE_ST_GETTEXT;
		
			}
			else if (t->internal.readAxisError.Error) {
				t->Error = 1;
				t->ErrorID = t->internal.readAxisError.ErrorID;
				strcpy(t->ErrorString, "Internal error getting axis status");
			}

			break;
		
	
		case AXERR_RETRIEVE_ST_GETTEXT:
			
			// Get the error text and handle it nicely
			if (t->internal.getErrorText.Done) {
			
				strncat4(	(UDINT)&(t->internal.errorString),
					(UDINT)&(t->internal.errorStringArray[0]),
					(UDINT)&(t->internal.errorStringArray[1]),
					(UDINT)&(t->internal.errorStringArray[2]),
					(UDINT)&(t->internal.errorStringArray[3]),
					sizeof(t->internal.errorString) - 1 );
				
				// Handle special errors based on error number 
				switch (t->internal.readAxisError.ErrorRecord.Number) {
   				
					// Loss of endless position data
					case 29297:

						t->internal.addErrorStatus= errcolAddError(	(UDINT)&(t->ErrorSourceName),
							(UDINT)&("Axis reference data has been lost.  Axis MUST be re-referenced before it is safe to continue."),
							t->internal.readAxisError.ErrorRecord.Number,
							(UDINT)&(t->Reset),
							(ErrorCollector_typ*)t->pErrorCollector );
					
						t->internal.pAcknowledge = (UDINT)&(t->Reset);
					
						break;
					
					// EnDat Battery failure 
					case 39006:
					case 39024: 
				
						t->internal.addErrorStatus= errcolAddError(	(UDINT)&(t->ErrorSourceName),
							(UDINT)&("Encoder battery power failure. Axis MUST be re-referenced before it is safe to continue."),
							t->internal.readAxisError.ErrorRecord.Number,
							(UDINT)&(t->ResetEnDat),
							(ErrorCollector_typ*)t->pErrorCollector );
					
						t->internal.pAcknowledge = (UDINT)&(t->ResetEnDat);
					
						break;
						
					// EnDat Battery Low 
					case 39010:
					case 39014:
					
						t->internal.addErrorStatus= errcolAddError(	(UDINT)&(t->ErrorSourceName),
							(UDINT)&("Encoder battery is low and MUST be replaced before the next power down. DO NOT FORGET! YOU WILL NOT SEE THIS ERROR AGAIN!"),
							t->internal.readAxisError.ErrorRecord.Number,
							(UDINT)&(t->Reset),
							(ErrorCollector_typ*)t->pErrorCollector );
				
						t->internal.pAcknowledge = (UDINT)&(t->Reset);
				
						break;
						
					// Other EnDat Alarms 
					case 39003:
					case 39004:
					case 39005:
					case 39007:
					case 39008:
					case 39009:
					case 39025:
					case 39026:
					case 39032:
					case 39033:
					case 39034:
					case 39035:
					case 39036:
					case 39037:
					case 39038:
					case 39039:
					case 39040:
					case 39041:
					case 39042:
					case 39043:
					case 39044:
					case 39045: t->internal.pAcknowledge = (UDINT)&(t->Reset); break;
				
					// Other 
					default: t->internal.pAcknowledge = (UDINT)&(t->Reset); break;
			
				}
				// switch (errornumber)
			
				// Add error to error collector
				t->internal.addErrorStatus= errcolAddError(	(UDINT)&(t->ErrorSourceName),
					(UDINT)&(t->internal.errorString),
					t->internal.readAxisError.ErrorRecord.Number,
					t->internal.pAcknowledge,
					(ErrorCollector_typ*)t->pErrorCollector );
				
				t->AxisError = 1;
				
				// Ack error and move on if the error collector isn't full
				if(t->internal.addErrorStatus != ERRCOL_ERR_FULLERRORS){
					t->internal.getErrorText.Execute = 0;
					t->internal.readAxisError.Acknowledge = 1;
					t->internal.retrieveState = AXERR_RETRIEVE_ST_IDLE;
				}			
			}
			else if (t->internal.getErrorText.Error) {
		
				t->internal.getErrorText.Execute=	0;
			
				// Add error to error collector
				t->internal.addErrorStatus= errcolAddError(	(UDINT)&(t->ErrorSourceName),
					(UDINT)&("Internal error getting error text"),
					t->internal.getErrorText.ErrorID,
					(UDINT)&(t->Reset),
					(ErrorCollector_typ*)t->pErrorCollector );
			
				// Ack error and move on if the error collector isn't full
				if(t->internal.addErrorStatus != ERRCOL_ERR_FULLERRORS){
					t->internal.getErrorText.Execute = 0;
					t->internal.readAxisError.Acknowledge = 1;
					t->internal.retrieveState = AXERR_RETRIEVE_ST_IDLE;
				}
  
			}
			
			break;
	}
	// switch(retrieveState)

	// FUBs
	t->internal.readAxisError.Axis = t->Axis;
	t->internal.readAxisError.Mode = mcNO_TEXT;
	t->internal.readAxisError.Enable = t->Enable;
	MC_BR_ReadAxisError(&(t->internal.readAxisError));

	MC_BR_GetErrorText(&(t->internal.getErrorText));
	
	
	//*************************************
	// Reset axis errors							
	//*************************************

	switch (t->internal.resetState) {
	
		case AXERR_RESET_ST_IDLE:
			
			t->internal.reset.Execute = 0;
			
			if (t->Reset) {
				t->Reset = 0;
				t->internal.resetState = AXERR_RESET_ST_RESET;
			}
			else if (t->ResetEnDat) {
				t->ResetEnDat = 0;
				t->internal.resetState = AXERR_RESET_ST_ENDAT;
			}
	
			break;
	
		
		case AXERR_RESET_ST_ENDAT:
	
			// Write 1 to ENCOD_CMD to clear EnDat error 
			t->internal.writeParIDText.Axis = t->Axis;
			t->internal.writeParIDText.ParID = ACP10PAR_ENCOD_CMD;
			strcpy(t->internal.writeParIDText.DataText, "1");
		
			t->internal.writeParIDText.Execute = 1;

			if (t->internal.writeParIDText.Done) {
				t->internal.writeParIDText.Execute = 0;
				t->internal.resetState = AXERR_RESET_ST_RESET;
			}
			else if (t->internal.writeParIDText.Error) {
  			
				t->internal.writeParIDText.Execute = 0;
			
				// Add error to error collector and move on 
				errcolAddError(	(UDINT)&(t->ErrorSourceName),
					(UDINT)&("Internal error resetting EnDat"),
					t->internal.writeParIDText.ErrorID,
					(UDINT)&(t->ResetEnDat),
					(ErrorCollector_typ*)t->pErrorCollector );
								
				t->internal.resetState=	AXERR_RESET_ST_RESET;
			
			}
			
			break;
		  
		
		case AXERR_RESET_ST_RESET:
		
			// Check for empty error queue before resetting axis 
			if ( 	(t->internal.readAxisError.Valid)
				&&	(t->internal.readAxisError.FunctionBlockErrorCount == 0)
				&&	(t->internal.readAxisError.AxisErrorCount == 0)
				&&	(t->internal.readAxisError.AxisWarningCount == 0)
			) {
				t->AxisError = 0;
				t->internal.reset.Execute = 1;
				t->internal.resetState = AXERR_RESET_ST_IDLE;
			}
			
			break;

	} 
	// switch(resetState)

	// FUBs 

	MC_BR_WriteParIDText(&(t->internal.writeParIDText));
	
	t->internal.reset.Axis = t->Axis;
	MC_Reset(&(t->internal.reset));
	
}
// function
