#include "shaderApp.h"

//--------------------------------------------------------------
void shaderApp::setup()
{
	ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetLogLevel("ofThread", OF_LOG_ERROR);
	ofSetVerticalSync(false);
	ofEnableAlphaBlending();
	
	doDrawInfo	= true;
		
	consoleListener.setup(this);
	
	sessionConfig.width = 1280;
	sessionConfig.height = 720;
	sessionConfig.framerate = 30;
	
	videoGrabber.setup(sessionConfig);
	filterCollection.setup();

	doShader = true;
	shader.load("shaderExample");
	
	fbo.allocate(sessionConfig.width, sessionConfig.height);
	fbo.begin();
		ofClear(0, 0, 0, 0);
	fbo.end();
	
	
		
}	

//--------------------------------------------------------------
void shaderApp::update()
{
	if (!doShader || !videoGrabber.isFrameNew())
	{
		return;
	}
	fbo.begin();
		ofClear(0, 0, 0, 0);
		shader.begin();
			shader.setUniformTexture("tex0", videoGrabber.getTextureReference(), videoGrabber.getTextureID());
			shader.setUniform1f("time", ofGetElapsedTimef());
			shader.setUniform2f("resolution", ofGetWidth(), ofGetHeight());
			videoGrabber.draw();
		shader.end();
	fbo.end();

}


//--------------------------------------------------------------
void shaderApp::draw(){
	
	if (doShader)
	{
		fbo.draw(0, 0);		
	}else 
	{
		videoGrabber.draw();
	}

	stringstream info;
	info << "APP FPS: " << ofGetFrameRate() << "\n";
	info << "Camera Resolution: " << videoGrabber.getWidth() << "x" << videoGrabber.getHeight()	<< " @ "<< videoGrabber.getFrameRate() <<"FPS"<< "\n";
	info << "CURRENT FILTER: " << filterCollection.getCurrentFilterName() << "\n";
	info << "SHADER ENABLED: " << doShader << "\n";
	//info <<	filterCollection.filterList << "\n";
	
	info << "\n";
	info << "Press e to increment filter" << "\n";
	info << "Press g to Toggle info" << "\n";
	info << "Press s to Toggle Shader" << "\n";
	
	if (doDrawInfo) 
	{
		ofDrawBitmapStringHighlight(info.str(), 100, 100, ofColor::black, ofColor::yellow);
	}
	
	//
}

//--------------------------------------------------------------
void shaderApp::keyPressed  (int key)
{
	ofLog(OF_LOG_VERBOSE, "%c keyPressed", key);
	
	if (key == 'e')
	{
		videoGrabber.setImageFilter(filterCollection.getNextFilter());
	}
	
	if (key == 'g')
	{
		doDrawInfo = !doDrawInfo;
	}
	
	if (key == 's')
	{
		doShader = !doShader;
	}

}

void shaderApp::onCharacterReceived(KeyListenerEventData& e)
{
	keyPressed((int)e.character);
}

