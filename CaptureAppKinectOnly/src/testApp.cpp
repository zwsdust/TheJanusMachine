#include "testApp.h"

void testApp::loadSettings() {
	ofxXmlSettings settings;
	settings.loadFile("settings.xml");
	
	settings.pushTag("serial");
	serialPort = settings.getValue("port", "");
	serialBaud = settings.getValue("baud", 0);
	settings.popTag();
	
	settings.pushTag("capture");
	fovMultiplier = settings.getValue("fovMultiplier", 0.f);
	captureFrameCount = settings.getValue("frameCount", 0);
	nearClipping = settings.getValue("nearClipping", 0);
	farClipping = settings.getValue("farClipping", 0);
	settings.popTag();
	
	settings.pushTag("timing");
	fadeInTime = settings.getValue("fadeIn", 0);
	recordTime = settings.getValue("record", 0);
	fadeOutTime = settings.getValue("fadeOut", 0);
	settings.popTag();
	
	settings.pushTag("transfer");		
	settings.pushTag("output");
	outputDirectory = settings.getValue("directory", "");
	outputPrefix =  settings.getValue("prefix", "");
	extension = settings.getValue("extension", "");
	
	settings.popTag();
	
	settings.pushTag("osc");
	oscIp = settings.getValue("ip", "");
	oscPort = settings.getValue("port", 0);
	settings.popTag();
	settings.popTag();
	
	FileStorage fs(ofToDataPath("homography.yml"), FileStorage::READ);
	fs["homography"] >> homography;
}

void testApp::setup() {
	//ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetVerticalSync(true);
	
	currentFrame = 0;
	recording = false;
	buttonPressedTime = 0;
	
	loadSettings();
	captureFrameInterval = recordTime / captureFrameCount;
	
	setupArduino();
	setupOsc();
	setupKinect();
}

testApp::~testApp() {
	for(int i = 0; i < captureFrameCount; i++) {
		delete depthBuffer[i];
		delete colorBuffer[i];
		delete rectifiedBuffer[i];
	}
}

void testApp::checkKinect() {
	if(!kinect.isConnected()) {
		kinect.open();
	}
}

void testApp::setupKinect() {
	kinect.init();
	ofxKinectCalibration::setClippingInCentimeters(nearClipping, farClipping);
	checkKinect();
	
	cout << "kinect is setup" << endl;
	
	if(kinect.isConnected()) {
		cout << "allocating " + ofToString(captureFrameCount) + " frames for kinect" << endl;
		depthBuffer.resize(captureFrameCount);
		colorBuffer.resize(captureFrameCount);
		rectifiedBuffer.resize(captureFrameCount);
		for(int i = 0; i < captureFrameCount; i++) {
			depthBuffer[i] = new ofPixels();
			colorBuffer[i] = new ofPixels();
			rectifiedBuffer[i] = new ofPixels();
			
			depthBuffer[i]->allocate(kinect.getWidth(), kinect.getHeight(), OF_IMAGE_GRAYSCALE);
			colorBuffer[i]->allocate(kinect.getWidth(), kinect.getHeight(), OF_IMAGE_COLOR);
			rectifiedBuffer[i]->allocate(rectifiedWidth, rectifiedHeight, OF_IMAGE_COLOR_ALPHA);
		}
		cout << "done allocating for kinect" << endl;
	}
}

void testApp::setupOsc() {
	osc.setup(oscIp, oscPort);
}

void testApp::setupArduino() {
	arduino.listDevices();
	arduinoReady = arduino.setup(serialPort, serialBaud);
	if(arduinoReady) {
		arduino.flush();
	}
}

void testApp::updateArduino() {
	curArduinoByte = (unsigned char) (fadeState * 255);
	if(arduinoReady) {
		if(arduino.available() > 0){
			char data[8];
			memset(data, 0, 8);
			arduino.readBytes((unsigned char*) data, 8);
			buttonPressed();
		}
		arduino.writeByte(curArduinoByte);
	}
}

void testApp::updateState() {
	unsigned long curTime = ofGetSystemTime();
	unsigned long diff = curTime - buttonPressedTime;
	if(diff < fadeInTime) {
		state = FADE_IN;
		fadeState = ofMap(diff, 0, fadeInTime, 0, 1);
	} else if(diff < fadeInTime + recordTime) {
		state = RECORD;
		fadeState = 1;
		recordingState = diff - fadeInTime;
	} else if(diff < fadeInTime + recordTime + fadeOutTime) {
		state = FADE_OUT;
		fadeState = .2; //ofMap(diff - fadeInTime - recordTime, 0, fadeOutTime, 1, 0);
	} else {
		state = IDLE;
		fadeState = 0;
	}
}

void testApp::sendOsc(string msg, string dir, string timestamp, int imageCount) {
	ofxOscMessage m;
	m.addStringArg(msg);
	m.addStringArg(dir);
	m.addStringArg(timestamp);
	m.addIntArg(imageCount);
	osc.sendMessage(m);
	
	cout << msg << endl;
}


void testApp::startFadeIn() {
	sendOsc("FadeInStarted");
	currentFrame = 0;
}

void testApp::startRecord() {
	sendOsc("ScanStarted");
	recording = true;
}

void testApp::startFadeOut() {
	sendOsc("CaptureStopped");
	
	recording = false;
	
	stringstream systemTime;
	systemTime << ofGetSystemTime();
	currentTimestamp = systemTime.str();
	currentDecodeFolder = string("decoded-") + outputPrefix + "-" + currentTimestamp + "/";
	ofDirectory::createDirectory(outputDirectory + currentDecodeFolder, false, true);
	totalFrameCount = currentFrame;
	
	// prepare/rectify images, takes maybe 1/2 second
	sendOsc("DecodeStarted");
	ofPixels rectified;
	rectified.allocate(rectifiedWidth, rectifiedHeight, OF_IMAGE_COLOR);
	for(int i = 0; i < totalFrameCount; i++) {
		warpPerspective(*colorBuffer[i], rectified, homography);
		unsigned char* rgb = rectified.getPixels();
		unsigned char* a = depthBuffer[i]->getPixels();
		unsigned char* rgba = rectifiedBuffer[i]->getPixels();
		int j = 0;
		int k = 0;
		int n = rectifiedWidth * rectifiedHeight;
		for(int m = 0; m < n; m++) {
			rgba[k++] = rgb[j++];
			rgba[k++] = rgb[j++];
			rgba[k++] = rgb[j++];
			rgba[k++] = a[m];
		}
	}
	sendOsc("DecodeEnded");
	
	// transfer images to disk
	sendOsc("TxStarted", currentDecodeFolder, currentTimestamp, totalFrameCount);
	Mat roi;
	for(int i = 0; i < totalFrameCount; i++) {		
		Mat cur = toCv(*rectifiedBuffer[i]);
		float w = cur.cols;
		float h = cur.rows;
		if(fovMultiplier != 1) {
			roi = Mat(cur, cv::Rect(ofMap(fovMultiplier, 1, 0, 0, w / 2),
															ofMap(fovMultiplier, 1, 0, 0, h / 2),
															ofMap(fovMultiplier, 1, 0, w, 0),
															ofMap(fovMultiplier, 1, 0, h, 0))).clone();
			resize(roi, cur, cur.size());
		}
		
		string curFilename = outputDirectory + currentDecodeFolder + ofToString(FRAME_START_INDEX + i) + "." + extension;
		ofSaveImage(*rectifiedBuffer[i], curFilename);
	}
	sendOsc("TxEnded", currentDecodeFolder, currentTimestamp, totalFrameCount);
}

void testApp::update() {
	ofBackground(128);
	
	// the ordering here is important for the image saving process
	// first, figure out if we're done
	updateState();
	
	// if we are done, the arduino will need to turn the light off
	updateArduino();
	
	// then we can go into startFadeOut() where we will save the images
	if(state != previousState) {
		switch(state) {
			case FADE_IN: startFadeIn(); break;
			case RECORD: startRecord(); break;
			case FADE_OUT: startFadeOut(); break;
		}
	}
	previousState = state;
	
	if(kinect.isConnected()) {
		checkKinect();
		kinect.update();
		
		if(recording) {
			bool needNewFrame = (currentFrame + 1) * captureFrameInterval < recordingState;
			if(needNewFrame && kinect.isConnected() && kinect.isFrameNew()) {
				depthBuffer[currentFrame]->setFromPixels(kinect.getDepthPixels(), kinect.getWidth(), kinect.getHeight(), OF_IMAGE_GRAYSCALE);
				colorBuffer[currentFrame]->setFromPixels(kinect.getPixels(), kinect.getWidth(), kinect.getHeight(), OF_IMAGE_COLOR);
				
				currentFrame++;
				
				if(currentFrame == captureFrameCount) {				
					recording = false;
				}
			}
		}
	}
}

void testApp::draw() {
	ofBackground(0);
	
	int tw = 320;
	int th = 240;
	
	ofSetColor(255);
	try {
		kinect.draw(0, 0, tw, th);
		kinect.drawDepth(0, th, tw, th);
	} catch (exception& e) {
		cout << ofGetTimestampString("%H:%M:%S:%i") << " kinect.draw() failed: " << e.what() << endl;
	}
	
	ofSetColor(ofColor::red);
	ofLine(tw / 2, 0, tw / 2, ofGetHeight());
	ofLine(0, th / 2, ofGetWidth(), th / 2);
	ofLine(0, th + th / 2, ofGetWidth(), th + th / 2);
	
	ofSetColor(curArduinoByte);
	ofCircle(80, 80, 30, 30);
	ofSetColor(255 - curArduinoByte);
	ofCircle(100, 100, 10, 10);
}

void testApp::exit() {
	kinect.close();
}

void testApp::buttonPressed() {
	if(state == IDLE) {
		buttonPressedTime = ofGetSystemTime();
	}
}

void testApp::keyPressed(int key) {
	if(key == ' ') {
		buttonPressed();
	}
	
	if(key == OF_KEY_UP) {
		farClipping++;
		ofxKinectCalibration::setClippingInCentimeters(nearClipping, farClipping);
		cout << farClipping << endl;
	}
	if(key == OF_KEY_DOWN) {
		farClipping--;
		ofxKinectCalibration::setClippingInCentimeters(nearClipping, farClipping);
		cout << farClipping << endl;
	}
}