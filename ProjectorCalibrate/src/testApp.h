#pragma once

#include "ofMain.h"
#include "Camera.h"
#include "LutFilter.h"

class testApp : public ofBaseApp{
public:
	void setup();
	void update();
	void draw();
	void keyPressed(int key);
	
	void getClipping(ofImage& img, ofImage& clipping);
	void drawPrep();
	void processAndSave();
	void drawCapture();
	
	void invert(vector<float>& f);
	void normalize(vector<float>& f);
	void smooth(vector<float>& f, float lambda);
	
	float currentShutter;
	int frame;
	bool captureMode;
	bool overexposed;
	int pass;
	
	ofxLibdc::Camera camera;
	ofImage img;
	
	static const int maxPasses = 8;
	vector< vector<ofImage> > images;
	bool saved;
	
	int wait;
};

