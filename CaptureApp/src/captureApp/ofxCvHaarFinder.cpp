#include "ofxCvHaarFinder.h"

ofxCvHaarFinder::ofxCvHaarFinder() {
	cascade = NULL;
	scaleHaar = 1.2;
	neighbors = 2;
}

ofxCvHaarFinder::ofxCvHaarFinder(const ofxCvHaarFinder& finder) {
	cascade = NULL;
	scaleHaar = finder.scaleHaar;
	neighbors = finder.neighbors;
	setup(finder.haarFile);
}

ofxCvHaarFinder::~ofxCvHaarFinder() {
	if(cascade != NULL)
		cvReleaseHaarClassifierCascade(&cascade);
}

void ofxCvHaarFinder::setScaleHaar(float scaleHaar) {
	this->scaleHaar = scaleHaar;
}

void ofxCvHaarFinder::setNeighbors(unsigned neighbors) {
	this->neighbors = neighbors;
}

void ofxCvHaarFinder::setup(string haarFileIn) {
	if(cascade != NULL)
		cvReleaseHaarClassifierCascade(&cascade);

	haarFile = haarFileIn;

	haarFile = ofToDataPath(haarFile);
	cascade = (CvHaarClassifierCascade*) cvLoad(haarFile.c_str(), 0, 0, 0);

	#ifdef HAAR_HACK
	// http://thread.gmane.org/gmane.comp.lib.opencv/16540/focus=17400
	// http://www.openframeworks.cc/forum/viewtopic.php?f=10&t=1853&hilit=haar
	ofxCvGrayscaleImage hack;
	hack.allocate(8, 8);
	CvMemStorage* storage = cvCreateMemStorage();
	cvHaarDetectObjects(hack.getCvImage(), cascade, storage, scaleHaar, 2, CV_HAAR_DO_CANNY_PRUNING);
	cvReleaseMemStorage(&storage);
	#endif

	if (!cascade)printf("Could not load Haar cascade: %s\n", haarFile.c_str());
}


float ofxCvHaarFinder::getWidth() {
	return img.width;
}

float ofxCvHaarFinder::getHeight() {
	return img.height;
}

bool ofxCvHaarFinder::ready() {
	return cascade != NULL;
}


int ofxCvHaarFinder::findHaarObjects(ofImage& input,
	int minWidth, int minHeight) {
	ofxCvColorImage color;
	ofxCvGrayscaleImage gray;
	color.allocate(input.width, input.height);
	gray.allocate(input.width, input.height);
	color = input.getPixels();
	gray = color;
	findHaarObjects(gray, minWidth, minHeight);
}

int ofxCvHaarFinder::findHaarObjects(ofxCvGrayscaleImage&  input,
	int minWidth, int minHeight) {
	return findHaarObjects(
		input, 0, 0, input.width, input.height,
		minWidth, minHeight);
}

int ofxCvHaarFinder::findHaarObjects(ofxCvGrayscaleImage&  input,
	ofRectangle& roi,
	int minWidth, int minHeight) {
	return findHaarObjects(
		input, (int) roi.x, (int) roi.y, (int) roi.width, (int) roi.height,
		minWidth, minHeight);
}

int ofxCvHaarFinder::findHaarObjects(ofxCvGrayscaleImage& input,
	int x, int y, int w, int h,
	int minWidth, int minHeight) {

	int nHaarResults = 0;

	if (cascade) {
		if (!blobs.empty())
			blobs.clear();

		// we make a copy of the input image here
		// because we need to equalize it.

		if (img.width == input.width && img.height == input.height) {
				img = input;
		} else {
				img.clear();
				img.allocate(input.width, input.height);
				img = input;
		}

		img.setROI(x, y, w, h);
		cvEqualizeHist(img.getCvImage(), img.getCvImage());
		CvMemStorage* storage = cvCreateMemStorage();

		/*
		Alternative modes:

		CV_HAAR_DO_CANNY_PRUNING
		Regions without edges are ignored.

		CV_HAAR_SCALE_IMAGE
		Scale the image rather than the detector
		(sometimes yields speed increases).

		CV_HAAR_FIND_BIGGEST_OBJECT
		Only return the largest result.

		CV_HAAR_DO_ROUGH_SEARCH
		When BIGGEST_OBJECT is enabled, stop at
		the first scale for which multiple results
		are found.
		*/

		CvSeq* haarResults = cvHaarDetectObjects(
				img.getCvImage(), cascade, storage, scaleHaar, neighbors, CV_HAAR_DO_CANNY_PRUNING,
				cvSize(minWidth, minHeight));

		nHaarResults = haarResults->total;

		for (int i = 0; i < nHaarResults; i++ ) {
			ofxCvBlob blob;

			CvRect* r = (CvRect*) cvGetSeqElem(haarResults, i);

			float area = r->width * r->height;
			float length = (r->width * 2) + (r->height * 2);
			float centerx	= (r->x) + (r->width / 2.0);
			float centery	= (r->y) + (r->height / 2.0);

			blob.area = fabs(area);
			blob.hole = area < 0 ? true : false;
			blob.length	= length;
			blob.boundingRect.x = r->x + x;
			blob.boundingRect.y = r->y + y;
			blob.boundingRect.width = r->width;
			blob.boundingRect.height = r->height;
			blob.centroid.x = centerx;
			blob.centroid.y = centery;
			blob.pts.push_back(ofPoint(r->x, r->y));
			blob.pts.push_back(ofPoint(r->x + r->width, r->y));
			blob.pts.push_back(ofPoint(r->x + r->width, r->y + r->height));
			blob.pts.push_back(ofPoint(r->x, r->y + r->height));

			blobs.push_back(blob);
		}

		cvReleaseMemStorage(&storage);
	}

	return nHaarResults;
}
