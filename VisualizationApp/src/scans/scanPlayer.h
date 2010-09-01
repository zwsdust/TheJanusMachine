
#pragma once

#include "ofMain.h"
//#include "ofx3DUtils.h"
#include "ofxDirList.h"
#include "threadedScanLoader.h"
//#include "Mesh.h"


class scanPlayer {

public:
	
	void setup();
	void loadDirectory(string pathName);
	
	ofImage *	depthFrames;
	ofImage *	imageFrames;
	bool	*	bLoaded;
	int			maxNumFrames;	// we should do this at some point
	
	int			totalNumFrames;
	int			currentFrame;
	
	ofTexture	imageData;
	
	threadedScanLoader TSL;
	
	void		update();
	void		draw();
	
	//Mesh		myMesh;
	int addedId[320*240];
	
	
	
};
