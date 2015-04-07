#include "StillCameraEngine.h"


StillCameraEngine::StillCameraEngine()
{
	didOpen		= false;
    doWriteFile = false;
    writeFileOnNextPass = false;
    doFillBuffer = false;
    bufferAvailable = false;
    render = NULL;
    camera = NULL;
    encoder = NULL;
    encoderOutputBuffer = NULL;
    sessionConfig = NULL;
    OMX_INIT_STRUCTURE(encoderOutputPortDefinition);
}   

inline
OMX_ERRORTYPE StillCameraEngine::encoderFillBufferDone(OMX_HANDLETYPE hComponent,
                                                       OMX_PTR pAppData,
                                                       OMX_BUFFERHEADERTYPE* pBuffer)
{	
    ofLogVerbose(__func__);
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    StillCameraEngine *grabber = static_cast<StillCameraEngine*>(pAppData);
    grabber->lock();
    grabber->bufferAvailable = true;
    grabber->unlock();
    return error;
}


void StillCameraEngine::setup(SessionConfig* sessionConfig_)
{
    sessionConfig = sessionConfig_;
    
    OMX_ERRORTYPE error = OMX_ErrorNone;
    
    OMX_CALLBACKTYPE cameraCallbacks;
    
    cameraCallbacks.EventHandler    = &StillCameraEngine::cameraEventHandlerCallback;
    cameraCallbacks.EmptyBufferDone	= &StillCameraEngine::nullEmptyBufferDone;
    cameraCallbacks.FillBufferDone	= &StillCameraEngine::nullFillBufferDone;
    
    error = OMX_GetHandle(&camera, OMX_CAMERA, this , &cameraCallbacks);
    if(error != OMX_ErrorNone) 
    {
        OMX_TRACE(error, "camera OMX_GetHandle FAIL");
    }
    
    configureCameraResolution();
    
}


inline
OMX_ERRORTYPE StillCameraEngine::cameraEventHandlerCallback(OMX_HANDLETYPE hComponent,
                                                     OMX_PTR pAppData,
                                                     OMX_EVENTTYPE eEvent,
                                                     OMX_U32 nData1,
                                                     OMX_U32 nData2,
                                                     OMX_PTR pEventData)
{
    /*ofLog(OF_LOG_VERBOSE, 
     "TextureEngine::%s - eEvent(0x%x), nData1(0x%lx), nData2(0x%lx), pEventData(0x%p)\n",
     __func__, eEvent, nData1, nData2, pEventData);*/
    StillCameraEngine *grabber = static_cast<StillCameraEngine*>(pAppData);
    //ofLogVerbose(__func__) << OMX_Maps::getInstance().eventTypes[eEvent];
    switch (eEvent) 
    {
        case OMX_EventParamOrConfigChanged:
        {
            
            return grabber->onCameraEventParamOrConfigChanged();
        }	
            
        case OMX_EventError:
        {
            OMX_TRACE((OMX_ERRORTYPE)nData1);
        }
        default: 
        {
            /*ofLog(OF_LOG_VERBOSE, 
             "TextureEngine::%s - eEvent(0x%x), nData1(0x%lx), nData2(0x%lx), pEventData(0x%p)\n",
             __func__, eEvent, nData1, nData2, pEventData);*/
            
            break;
        }
    }
    return OMX_ErrorNone;
}

string printPortDef(OMX_PARAM_PORTDEFINITIONTYPE portDef)
{
    stringstream info;
    info << "nFrameWidth: "  << portDef.format.image.nFrameWidth << endl;
    info << "nFrameHeight: "  << portDef.format.image.nFrameHeight << endl;
    info << "nStride: "  << portDef.format.image.nStride << endl;
    info << "nSliceHeight: "  << portDef.format.image.nSliceHeight << endl;
    info << "bFlagErrorConcealment: "  << portDef.format.image.bFlagErrorConcealment << endl;
    info << "Color Format: " << OMX_Maps::getInstance().getColorFormat(portDef.format.image.eColorFormat) << endl;
    info << "Compression Format: "    << OMX_Maps::getInstance().getImageCoding(portDef.format.image.eCompressionFormat) << "\n";
    
    ofLogVerbose(__func__) << "info: \n" << info.str();
    return info.str();
}

inline
OMX_ERRORTYPE StillCameraEngine::configureCameraResolution()
{
    
    OMX_ERRORTYPE error;
    error = DisableAllPortsForComponent(&camera);
	OMX_TRACE(error);
    
    
	OMX_CONFIG_REQUESTCALLBACKTYPE cameraCallback;
	OMX_INIT_STRUCTURE(cameraCallback);
	cameraCallback.nPortIndex	=	OMX_ALL;
	cameraCallback.nIndex		=	OMX_IndexParamCameraDeviceNumber;
	cameraCallback.bEnable		=	OMX_TRUE;
	
	error = OMX_SetConfig(camera, OMX_IndexConfigRequestCallback, &cameraCallback);
	OMX_TRACE(error);
    
	//Set the camera (always 0)
	OMX_PARAM_U32TYPE device;
	OMX_INIT_STRUCTURE(device);
	device.nPortIndex	= OMX_ALL;
	device.nU32			= 0;
	
	error = OMX_SetParameter(camera, OMX_IndexParamCameraDeviceNumber, &device);
	OMX_TRACE(error);
    
	//Set the resolution
	OMX_PARAM_PORTDEFINITIONTYPE stillPortConfig;
	OMX_INIT_STRUCTURE(stillPortConfig);
	stillPortConfig.nPortIndex = CAMERA_STILL_OUTPUT_PORT;
	
	error =  OMX_GetParameter(camera, OMX_IndexParamPortDefinition, &stillPortConfig);
    OMX_TRACE(error);
	if(error == OMX_ErrorNone) 
	{
        ofLogVerbose() << "BEFORE SET";
        printPortDef(stillPortConfig);
        stillPortConfig.format.image.nFrameWidth		= sessionConfig->width;
        stillPortConfig.format.image.nFrameHeight       = sessionConfig->height;

        stillPortConfig.format.video.nStride			= sessionConfig->width;
        //below works but leaving it at default 0
        //stillPortConfig.format.video.nSliceHeight	= round(sessionConfig->height / 16) * 16;
        
        error =  OMX_SetParameter(camera, OMX_IndexParamPortDefinition, &stillPortConfig);
        OMX_TRACE(error);
        error =  OMX_GetParameter(camera, OMX_IndexParamPortDefinition, &stillPortConfig);
        ofLogVerbose() << "AFTER SET";
        printPortDef(stillPortConfig);
	}
    return error;
}


inline
void StillCameraEngine::threadedFunction()
{
	while (isThreadRunning()) 
	{
        if(bufferAvailable)
        {
            
            
            
            recordingFileBuffer.append((const char*) encoderOutputBuffer->pBuffer + encoderOutputBuffer->nOffset, 
                                       encoderOutputBuffer->nFilledLen);
            
            
            ofLogVerbose(__func__) << recordingFileBuffer.size();
            bool endOfFrame = (encoderOutputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME);
            bool endOfStream = (encoderOutputBuffer->nFlags & OMX_BUFFERFLAG_EOS);
            
            ofLogVerbose(__func__) << "OMX_BUFFERFLAG_ENDOFFRAME: " << endOfFrame;
            ofLogVerbose(__func__) << "OMX_BUFFERFLAG_EOS: " << endOfStream;
            
            if(endOfStream)
            {
                doFillBuffer = false;
                writeFile();
            }else
            {
                doFillBuffer = true;
            }
            
        }
        if(doFillBuffer)
        {
            doFillBuffer	= false;
            bufferAvailable = false;
            OMX_ERRORTYPE error = OMX_FillThisBuffer(encoder, encoderOutputBuffer);
            OMX_TRACE(error);
        }
        
       
        
        
    }
}

inline
OMX_ERRORTYPE StillCameraEngine::onCameraEventParamOrConfigChanged()
{
    
    OMX_ERRORTYPE error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error, "camera->OMX_StateIdle");

    
    OMX_FRAMESIZETYPE frameSizeConfig;
    OMX_INIT_STRUCTURE(frameSizeConfig);
    frameSizeConfig.nPortIndex = OMX_ALL;
    
    OMX_PARAM_SENSORMODETYPE sensorMode;
    OMX_INIT_STRUCTURE(sensorMode);
    sensorMode.nPortIndex = OMX_ALL;
    sensorMode.sFrameSize = frameSizeConfig;
    error =OMX_GetParameter(camera, OMX_IndexParamCommonSensorMode, &sensorMode);
    
    OMX_TRACE(error);
    
    sensorMode.bOneShot = OMX_TRUE;
    error =OMX_SetParameter(camera, OMX_IndexParamCommonSensorMode, &sensorMode);

    OMX_TRACE(error);
    
    OMX_CONFIG_CAMERASENSORMODETYPE sensorConfig;
    OMX_INIT_STRUCTURE(sensorConfig);
    sensorConfig.nPortIndex = OMX_ALL;
    //sensorConfig.nModeIndex = 0;
    error =  OMX_GetParameter(camera, OMX_IndexConfigCameraSensorModes, &sensorConfig);
    OMX_TRACE(error);

    OMX_CONFIG_PORTBOOLEANTYPE cameraStillOutputPortConfig;
    OMX_INIT_STRUCTURE(cameraStillOutputPortConfig);
    cameraStillOutputPortConfig.nPortIndex = CAMERA_STILL_OUTPUT_PORT;
    cameraStillOutputPortConfig.bEnabled = OMX_TRUE;
    
    error =OMX_SetParameter(camera, OMX_IndexConfigPortCapturing, &cameraStillOutputPortConfig);	
    OMX_TRACE(error);
    
    
    //Set up renderer
    OMX_CALLBACKTYPE renderCallbacks;
    renderCallbacks.EventHandler	= &StillCameraEngine::nullEventHandlerCallback;
    renderCallbacks.EmptyBufferDone	= &StillCameraEngine::nullEmptyBufferDone;
    renderCallbacks.FillBufferDone	= &StillCameraEngine::nullFillBufferDone;
    

    OMX_GetHandle(&render, OMX_VIDEO_RENDER, this , &renderCallbacks);
    DisableAllPortsForComponent(&render);
    
    
    
    //Set renderer to Idle
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error);
    
    OMX_CONFIG_DISPLAYREGIONTYPE region;
    
    OMX_INIT_STRUCTURE(region);
    region.nPortIndex = VIDEO_RENDER_INPUT_PORT; /* Video render input port */
    
    region.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_DEST_RECT | OMX_DISPLAY_SET_FULLSCREEN);
    
    region.fullscreen = OMX_FALSE;
    region.noaspect = OMX_TRUE;
    
    region.dest_rect.x_offset = 0;
    region.dest_rect.y_offset = 0;
    region.dest_rect.width	= sessionConfig->width;
    region.dest_rect.height = sessionConfig->height;
    
    error  = OMX_SetParameter(render, OMX_IndexConfigDisplayRegion, &region);
    
    OMX_TRACE(error, "render OMX_IndexConfigDisplayRegion");
    
    
    //Create encoder
    
    OMX_CALLBACKTYPE encoderCallbacks;
    encoderCallbacks.EventHandler		= &StillCameraEngine::nullEventHandlerCallback;
    encoderCallbacks.EmptyBufferDone	= &StillCameraEngine::nullEmptyBufferDone;
    encoderCallbacks.FillBufferDone		= &StillCameraEngine::encoderFillBufferDone;
    
    error =OMX_GetHandle(&encoder, OMX_IMAGE_ENCODER, this , &encoderCallbacks);
    OMX_TRACE(error);
    
    

    
    configureEncoder();
    
    
    
    
    //Create camera->render Tunnel
    error = OMX_SetupTunnel(camera, CAMERA_PREVIEW_PORT, render, VIDEO_RENDER_INPUT_PORT);
    OMX_TRACE(error);
    
    //Create camera->encoder Tunnel
    error = OMX_SetupTunnel(camera, CAMERA_STILL_OUTPUT_PORT, encoder, IMAGE_ENCODER_INPUT_PORT);
    OMX_TRACE(error);
    
    //Enable camera output port
    error = OMX_SendCommand(camera, OMX_CommandPortEnable, CAMERA_STILL_OUTPUT_PORT, NULL);
    OMX_TRACE(error);
    
    //Enable camera preview port
    error = OMX_SendCommand(camera, OMX_CommandPortEnable, CAMERA_PREVIEW_PORT, NULL);
    OMX_TRACE(error);
    
    //Enable encoder input port
    error = OMX_SendCommand(encoder, OMX_CommandPortEnable, IMAGE_ENCODER_INPUT_PORT, NULL);
    OMX_TRACE(error);    
 
    //Enable encoder output port
    error = OMX_SendCommand(encoder, OMX_CommandPortEnable, IMAGE_ENCODER_OUTPUT_PORT, NULL);
    OMX_TRACE(error); 
    
    //Set encoder to Idle
    error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error);
    
    error =  OMX_AllocateBuffer(encoder, &encoderOutputBuffer, IMAGE_ENCODER_OUTPUT_PORT, NULL, encoderOutputPortDefinition.nBufferSize);
    OMX_TRACE(error, "encode OMX_AllocateBuffer");
    
    //Enable render input port
    error = OMX_SendCommand(render, OMX_CommandPortEnable, VIDEO_RENDER_INPUT_PORT, NULL);
    OMX_TRACE(error);
    
    
    //Start renderer
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    OMX_TRACE(error);
    
    
    //Start encoder
    error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    OMX_TRACE(error);
    
    //Start camera
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateExecuting, NULL);
    OMX_TRACE(error);
    
   
    
    bool doThreadBlocking	= true;
    startThread(doThreadBlocking);
    
    error = OMX_FillThisBuffer(encoder, encoderOutputBuffer);
    
    OMX_TRACE(error);
    
    didOpen = true;
    return error;
}

bool StillCameraEngine::writeFile()
{
    
    bool result = ofBufferToFile(ofToDataPath(ofGetTimestampString()+".jpg", true), recordingFileBuffer);
    doWriteFile = false;
    stopThread();
    return result;
}


inline
OMX_ERRORTYPE StillCameraEngine::configureEncoder()
{
    
    OMX_ERRORTYPE error;
    
    error = DisableAllPortsForComponent(&encoder);
    OMX_TRACE(error);
    
    // Encoder input port definition is done automatically upon tunneling
    
    // Configure video format emitted by encoder output port

    encoderOutputPortDefinition.nPortIndex = IMAGE_ENCODER_OUTPUT_PORT;
    error =OMX_GetParameter(encoder, OMX_IndexParamPortDefinition, &encoderOutputPortDefinition);
    OMX_TRACE(error);
    if (error == OMX_ErrorNone) 
    {
        ofLogVerbose() << "ENCODER BEFORE SET";
        printPortDef(encoderOutputPortDefinition);
        encoderOutputPortDefinition.format.image.nFrameWidth = sessionConfig->width;
        encoderOutputPortDefinition.format.image.nFrameHeight= sessionConfig->height;
        encoderOutputPortDefinition.format.image.nStride = sessionConfig->width; 
        encoderOutputPortDefinition.format.image.eColorFormat = OMX_COLOR_FormatUnused;
        encoderOutputPortDefinition.format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
        error =OMX_SetParameter(encoder, OMX_IndexParamPortDefinition, &encoderOutputPortDefinition);
        OMX_TRACE(error);
        ofLogVerbose() << "ENCODER AFTER SET";
        printPortDef(encoderOutputPortDefinition);
        


        
    }
    return error;
    
}

StillCameraEngine::~StillCameraEngine()
{
    if(didOpen)
    {
        closeEngine();
    }
}

inline
void StillCameraEngine::closeEngine()
{
    
    if(isThreadRunning())
    {
        stopThread();
    }
    OMX_ERRORTYPE error;
    error =  OMX_SendCommand(camera, OMX_CommandFlush, CAMERA_STILL_OUTPUT_PORT, NULL);
    OMX_TRACE(error, "camera: OMX_CommandFlush");
    
    error = DisableAllPortsForComponent(&camera, "camera");
    OMX_TRACE(error, "DisableAllPortsForComponent: camera");
    

    //OMX_StateIdle
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error, "camera->OMX_StateIdle");

    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error, "render->OMX_StateIdle");
    
   
    error =  OMX_SendCommand(encoder, OMX_CommandFlush, IMAGE_ENCODER_INPUT_PORT, NULL);
    OMX_TRACE(error, "encoder: OMX_CommandFlush IMAGE_ENCODER_INPUT_PORT");
    error =  OMX_SendCommand(encoder, OMX_CommandFlush, IMAGE_ENCODER_OUTPUT_PORT, NULL);
    OMX_TRACE(error, "encoder: OMX_CommandFlush IMAGE_ENCODER_OUTPUT_PORT");
    
    error = DisableAllPortsForComponent(&encoder, "encoder");
    OMX_TRACE(error, "DisableAllPortsForComponent encoder");
    
    error = OMX_FreeBuffer(encoder, IMAGE_ENCODER_OUTPUT_PORT, encoderOutputBuffer);
    OMX_TRACE(error, "encoder->OMX_StateIdle");
    error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateIdle, NULL);
    OMX_TRACE(error, "encoder->OMX_StateIdle");
    
    
    OMX_STATETYPE encoderState;
    error = OMX_GetState(encoder, &encoderState);
    OMX_TRACE(error, "encoderState: "+ getStateString(encoderState));
 

    
    //OMX_StateLoaded
    error = OMX_SendCommand(camera, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    OMX_TRACE(error, "camera->OMX_StateLoaded");
    
    error = OMX_SendCommand(render, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    OMX_TRACE(error, "render->OMX_StateLoaded");
    
    error = OMX_SendCommand(encoder, OMX_CommandStateSet, OMX_StateLoaded, NULL);
    OMX_TRACE(error, "encoder->OMX_StateLoaded");

    
    //OMX_FreeHandle
    error = OMX_FreeHandle(camera);
    OMX_TRACE(error, "OMX_FreeHandle(camera)");
    
    error =  OMX_FreeHandle(render);
    OMX_TRACE(error, "OMX_FreeHandle(render)");
   
    error = OMX_FreeHandle(encoder);
    OMX_TRACE(error, "OMX_FreeHandle(encoder)"); 
    
    sessionConfig = NULL;
    didOpen = false;

}
