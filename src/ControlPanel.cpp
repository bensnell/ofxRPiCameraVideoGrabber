#include "ControlPanel.h"


ofParameter<bool> createBoolean(ofXml& xml)
{
    string childName = xml.getName();
    ofParameter<bool> item;
    item.set(childName, xml.getBoolValue());
    return item;
}

ofParameterGroup* createParameterGroup(ofXml& xml)
{
    ofParameterGroup* parameterGroup = new ofParameterGroup();
    string elementName = xml.getName();
	xml.setAttribute("type", "group");
    parameterGroup->setName(elementName);
    int numElementChildren = xml.getNumChildren();
    for(int i=0; i<numElementChildren; i++)
    {
        
        xml.setToChild(i);
			ofParameter<bool> item = createBoolean(xml);
			parameterGroup->add(item);
        xml.setToParent();
        
    }
    return parameterGroup;
}



ControlPanel::ControlPanel()
{
	localPort = 6666;
	remotePort = 7777;
	rpiCameraVideoGrabber = NULL;
	serializer = new ofXml();
}

void ControlPanel::onParameterGroupChanged(ofAbstractParameter & param)
{
	ofLogVerbose(__func__) << "param.getName: " << param.getName();
	sync->sender.sendParameter(param);
}

void ControlPanel::onVideoCodingNamesChanged(ofAbstractParameter & param)
{
	ofLogVerbose(__func__) << "param.getName(): " << param.getName();
	ofRemoveListener(videoCodingNames.parameterChangedE, this, &ControlPanel::onVideoCodingNamesChanged);	
	for (int i=0; i<videoCodingNames.size(); i++) 
	{
		if(videoCodingNames.getName(i) != param.getName())
		{
			videoCodingNames.getBool(i) = false;
			//ofLogVerbose(__func__) << "setting " << videoCodingNames.getName(i) << " to false";
		}
		
	}
	ofAddListener(videoCodingNames.parameterChangedE, this, &ControlPanel::onVideoCodingNamesChanged);	

}
void ControlPanel::onExposureControlNamesChanged(ofAbstractParameter & param)
{
	ofLogVerbose(__func__) << "onExposureControlNamesChanged";
	ofRemoveListener(exposureControlNames.parameterChangedE, this, &ControlPanel::onExposureControlNamesChanged);
	for (int i=0; i<exposureControlNames.size(); i++) 
	{
		if(exposureControlNames.getName(i) != param.getName())
		{
			//ofLogVerbose(__func__) << "setting " << exposureControlNames.getName(i) << " to false";
			exposureControlNames.getBool(i) = false;
		}
	}
	ofAddListener(exposureControlNames.parameterChangedE, this, &ControlPanel::onExposureControlNamesChanged);
}


void ControlPanel::setup(ofxRPiCameraVideoGrabber* rpiCameraVideoGrabber)
{
	
	ofParameter<int> sharpness;
	ofParameter<int> contrast;
	ofParameter<int> brightness;
	ofParameter<int> saturation;
	ofParameter<bool> frameStabilizationEnabled;
	ofParameter<bool> colorEnhancementEnabled;
	ofParameter<bool> ledEnabled;
	
	this->rpiCameraVideoGrabber = rpiCameraVideoGrabber;
	parameters.setName("root");
    
    sharpness.set("sharpness", rpiCameraVideoGrabber->getSharpness(), -100, 100);
	sharpness.addListener(this, &ControlPanel::onSharpnessChanged);
	
    contrast.set("contrast", rpiCameraVideoGrabber->getContrast(), -100, 100);
	contrast.addListener(this, &ControlPanel::onContrastChanged);
	
    brightness.set("brightness", rpiCameraVideoGrabber->getBrightness(), 0, 100);
	brightness.addListener(this, &ControlPanel::onBrightnessChanged);
	
    saturation.set("saturation", rpiCameraVideoGrabber->getSaturation(), -100, 100);
	saturation.addListener(this, &ControlPanel::onSaturationChanged);
	
    frameStabilizationEnabled.set("FrameStabilization", false);
	frameStabilizationEnabled.addListener(this, &ControlPanel::onFrameStabilizationChanged);
	
    colorEnhancementEnabled.set("ColorEnhancement", false);
	colorEnhancementEnabled.addListener(this, &ControlPanel::onColorEnhancementChanged);
	
    ledEnabled.set("LED", false);
	ledEnabled.addListener(this, &ControlPanel::onLEDEnabledChanged);
	
	size_t i=0;
	videoCodingNames.setName("videoCodingNames");
	for (i=0; i<rpiCameraVideoGrabber->omxMaps.videoCodingNames.size(); i++)
	{
		ofParameter<bool> item;
		item.set(rpiCameraVideoGrabber->omxMaps.videoCodingNames[i], false);
		videoCodingNames.add(item);
	}
	
	ofAddListener(videoCodingNames.parameterChangedE, this, &ControlPanel::onVideoCodingNamesChanged);	
	
	
	
	exposureControlNames.setName("exposureControlNames");
	for (i=0; i<rpiCameraVideoGrabber->omxMaps.exposureControlNames.size(); i++)
	{
		ofParameter<bool> item;
		item.set(rpiCameraVideoGrabber->omxMaps.exposureControlNames[i], false);
		exposureControlNames.add(item);
	}
	
	meteringNames.setName("meteringNames");	
	for (i=0; i<rpiCameraVideoGrabber->omxMaps.meteringNames.size(); i++)
	{
		ofParameter<bool> item;
		item.set(rpiCameraVideoGrabber->omxMaps.meteringNames[i], false);
		meteringNames.add(item);
	}
	ofAddListener(exposureControlNames.parameterChangedE, this, &ControlPanel::onExposureControlNamesChanged);	
	whiteBalanceNames.setName("whiteBalanceNames");	
	for (i=0; i<rpiCameraVideoGrabber->omxMaps.whiteBalanceNames.size(); i++)
	{
		ofParameter<bool> item;
		item.set(rpiCameraVideoGrabber->omxMaps.whiteBalanceNames[i], false);
		whiteBalanceNames.add(item);
	}
	
	imageFilterNames.setName("imageFilterNames");
	for (i=0; i<rpiCameraVideoGrabber->omxMaps.imageFilterNames.size(); i++)
	{
		ofParameter<bool> item;
		item.set(rpiCameraVideoGrabber->omxMaps.imageFilterNames[i], false);
		imageFilterNames.add(item);
	}
	
	parameters.add(frameStabilizationEnabled);
	parameters.add(sharpness);
	parameters.add(brightness);
	parameters.add(contrast);
	parameters.add(saturation);
	parameters.add(colorEnhancementEnabled);
	parameters.add(ledEnabled);
		parameters.add(videoCodingNames);
		parameters.add(exposureControlNames);
		parameters.add(meteringNames);
		parameters.add(whiteBalanceNames);
		parameters.add(imageFilterNames);
		
	
	gui.setup(parameters);
  
	guiParamGroup = (ofParameterGroup*)&gui.getParameter();

	sync = new OSCParameterSync();
	sync->setup(*guiParamGroup, localPort, "JVCTOWER.local", remotePort);
	
	saveXML();
	
	//ofLogVerbose(__func__) << "parameters: " << parameters.toString();
	ofAddListener(parameters.parameterChangedE, this, &ControlPanel::onParameterGroupChanged);
	
	gui.setPosition(204, 44);
	
	ofAddListener(ofEvents().update, this, &ControlPanel::onUpdate);
	
}



void ControlPanel::saveXML()
{
	serializer->serialize(parameters);
	
	ofXml xmlParser(*serializer);
	ofLogVerbose(__func__) << "xmlParser toString: " << xmlParser.toString();
	
	int numRootChildren =  xmlParser.getNumChildren();
    for(int i=0; i<numRootChildren; i++)
    {
        xmlParser.setToChild(i);
		int numElementChildren = xmlParser.getNumChildren();
		if (numElementChildren>0)
		{
			
			ofParameterGroup* parameterGroupPtr = createParameterGroup(xmlParser);
			for(int j=0; j<numElementChildren; j++)
			{
				xmlParser.setToChild(j);
					ofParameterGroup parameterGroup = *parameterGroupPtr;
					createXMLFromParam(parameterGroup[xmlParser.getName()], xmlParser);
				xmlParser.setToParent();
			}
			
		}else 
		{
			createXMLFromParam(parameters[xmlParser.getName()], xmlParser);
		}
		
        xmlParser.setToParent();
    }
	
	ofLogVerbose(__func__) << "xmlParser processed: " << xmlParser.toString();
	string filename = ofToDataPath("DocumentRoot/modified.xml", true);
	xmlParser.save(filename);
	
	
}

void ControlPanel::createXMLFromParam(ofAbstractParameter& parameter, ofXml& xml)
{
	
	string parameterName = xml.getName();
	if(parameter.type()==typeid(ofParameter<int>).name())
	{
		xml.setAttribute("type", "int");
		xml.setAttribute("min", ofToString(parameter.cast<int>().getMin()));
		xml.setAttribute("max", ofToString(parameter.cast<int>().getMax()));
	}
	else if(parameter.type()==typeid(ofParameter<float>).name())
	{
		
		xml.setAttribute("type", "float");
		xml.setAttribute("min", ofToString(parameter.cast<float>().getMin()));
		xml.setAttribute("max", ofToString(parameter.cast<float>().getMax()));
	}
	else if(parameter.type()==typeid(ofParameter<bool>).name())
	{
		xml.setAttribute("type", "bool");
	}
	else if(parameter.type()==typeid(ofParameter<string>).name())
	{
		xml.setAttribute("type", "string");
	}
	
}

void ControlPanel::onUpdate(ofEventArgs &args)
{
	sync->update();
}

void ControlPanel::increaseContrast()
{
	guiParamGroup->get("contrast").cast<int>()++;
//ofAbstractParameter randomName = videoCodingNames.get((int));
	//ofLogVerbose(__func__) << "randomName.getName(): " << randomName.getName();
	videoCodingNames.getBool(ofRandom(videoCodingNames.size()-1)) = true;
	
}

void ControlPanel::onSharpnessChanged(int & sharpness)
{
	//ofLogVerbose(__func__) << sharpness;
	rpiCameraVideoGrabber->setSharpness(sharpness);
}

void ControlPanel::onContrastChanged(int & contrast)
{
	//ofLogVerbose(__func__) << contrast;
	rpiCameraVideoGrabber->setContrast(contrast);
}

void ControlPanel::onBrightnessChanged(int & brightness)
{
	//ofLogVerbose(__func__) << brightness;
	rpiCameraVideoGrabber->setBrightness(brightness);
}

void ControlPanel::onSaturationChanged(int & saturation)
{
	//ofLogVerbose(__func__) << saturation;
	rpiCameraVideoGrabber->setSaturation(saturation);
}
void ControlPanel::onFrameStabilizationChanged(bool & doFrameStabilization)
{
	rpiCameraVideoGrabber->setFrameStabilization(doFrameStabilization);
}

void ControlPanel::onColorEnhancementChanged(bool & doColorEnhancement)
{
	rpiCameraVideoGrabber->setColorEnhancement(doColorEnhancement);
}

void ControlPanel::onLEDEnabledChanged(bool & doEnableLED)
{
	rpiCameraVideoGrabber->setLEDStatus(doEnableLED);
}
