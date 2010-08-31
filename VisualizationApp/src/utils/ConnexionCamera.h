#pragma once

#include "ofxConnexion.h"
#include "ofxVectorMath.h"
#include "Particle.h"
#include "ofMain.h"

const ofxVec3f xunit3f(1, 0, 0), yunit3f(0, 1, 0), zunit3f(0, 0, 1);

class ConnexionCamera {
public:
	ConnexionCamera();
	void draw(float mouseX, float mouseY);
	void addRotation(ofxQuaternion rotation);
protected:
	float curZoom, minZoom, maxZoom;
	float zoomSpeed;
	float rotationMomentum, rotationSpeed;
	ofxQuaternion lastOrientationVelocity, curOrientation;
};