#include <jni.h>
#include "LTKInkFileReader.h"
#include "LTKLipiEngineInterface.h"
#include "LipiEngineModule.h"
#include "lipiengine.h"
#include "LTKMacros.h"
#include "LTKInc.h"
#include "LTKTypes.h"
#include "LTKTrace.h"
#include <string>

// Allow logging support
#include <android/log.h>

#define LOGD(LOG_TAG, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGV(LOG_TAG, ...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGE(LOG_TAG, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Define some tags
#define LOG_JNI "JNI"

// Make random JNIEXPORT errors go away in eclipse
#ifndef JNIEXPORT
#define JNIEXPORT
#endif
#ifndef JNICALL
#define JNICALL
#endif

#define CONFIDENCE_THRESHOLD 0.002f
#define NUMBER_OF_RESULTS 3

// Capture device settings
float gXDpi = 265.0f;
float gYDpi = 265.0f;
float gLatency = 0.0f; // ?? Look up what this is
float gSamplingRate = 1.0f; // ?? Look up what this is
bool gUseUniformSamplingRate = true;

// LipiTk engine vars
LTKLipiEngineInterface* gEngine;
LTKShapeRecognizer* gShapeReco = NULL;

extern "C" {
	JNIEXPORT void JNICALL Java_rcos_main_recognition_LipiTKJNIInterface_initializeNative(JNIEnv *env, jobject this_object, jstring lipiDirectory, jstring project);
	JNIEXPORT jobjectArray JNICALL Java_rcos_main_recognition_LipiTKJNIInterface_recognizeNative(JNIEnv *env, jobject this_object, jobjectArray strokeList, jint numJStrokes);
}

JNIEXPORT void JNICALL Java_rcos_main_recognition_LipiTKJNIInterface_initializeNative(JNIEnv *env, jobject this_object, jstring lipiDirectory, jstring project) {
	int result;
	LOGD(LOG_JNI, "Getting Lipi Location");
	char* lipitkLocation = (char*)env->GetStringUTFChars(lipiDirectory, NULL); // ?? Need to release this

	LOGD(LOG_JNI, "Creating Engine");
	// Load the engine
	LTKLipiEngineInterface* gEngine = createLTKLipiEngine();
	gEngine->setLipiRootPath(lipitkLocation);

	LOGD(LOG_JNI, "Initializing Engine");
	// Initialize the engine
	result = gEngine->initializeLipiEngine();
	if(result != SUCCESS) {
		LOGE(LOG_JNI, "Error initializing LipiTk Engine");
		return; // Possibly should delete some stuff before returning
	}

	LOGD(LOG_JNI, "Loading Shape Recognizer");
	// Load the shape recognizer
	std::string projectStr = std::string((char*)env->GetStringUTFChars(project, NULL));
	gEngine->createShapeRecognizer(projectStr, &gShapeReco);
	if(gShapeReco == NULL) {
		LOGE(LOG_JNI, "Error creating a shape recognizer");
		return; // Possibly should delete some stuff before returning
	}

	LOGD(LOG_JNI, "Loading Model Data");
	// Load the model data for the shape recognizer
	result = gShapeReco->loadModelData();
	if(result != SUCCESS) {
		LOGE(LOG_JNI, "Error loading model data");
		return; // Possibly should delete some stuff before returning
	}
	LOGD(LOG_JNI,"Successfully Initialized LIPI TK");
}

JNIEXPORT jobjectArray JNICALL Java_rcos_main_recognition_LipiTKJNIInterface_recognizeNative(JNIEnv *env, jobject this_object, jobjectArray strokeList, jint numJStrokes) {
	jclass strokeClass = env->FindClass("rcos/main/Stroke");
	if(strokeClass == NULL)
		LOGE(LOG_JNI, "strokeClass is null...");
	LOGD(LOG_JNI, "Getting the getNumPoints method");
	jmethodID getNumPointsMethodID = env->GetMethodID(strokeClass, "getNumberOfPoints", "()I");
	LOGD(LOG_JNI, "Getting the getPointAt method");
	jmethodID getPointsAtMethodID = env->GetMethodID(strokeClass, "getPointAt", "(I)Landroid/graphics/PointF;");

	// Get Point class methods
	LOGD(LOG_JNI, "Getting the PointF class");
	jclass pointFClass = env->FindClass("android/graphics/PointF");
	LOGD(LOG_JNI, "Getting the xField");
	jfieldID xFieldID = env->GetFieldID(pointFClass, "x", "F");
	LOGD(LOG_JNI, "Getting the yField");
	jfieldID yFieldID = env->GetFieldID(pointFClass, "y", "F");

	// Get the number of strokes
	int numStrokes = (int)numJStrokes;

	LOGD(LOG_JNI, "Number of strokes: %d", numStrokes);

	vector<int> outSubSetOfClasses;

	vector<LTKShapeRecoResult> outResults;
	outResults.reserve(NUMBER_OF_RESULTS);

	LTKScreenContext screenContext;
	LTKCaptureDevice ltkcapturedevice;

	ltkcapturedevice.setXDPI(gXDpi);
	ltkcapturedevice.setYDPI(gYDpi);

	// ?? THIS NEEDS TO BE GIVEN PROPER VALUES
	screenContext.setBboxLeft(0);
	screenContext.setBboxBottom(0);
	screenContext.setBboxRight(480);
	screenContext.setBboxTop(800);

	LTKTraceGroup traceGroup;
	std::vector<float> pointVec;
	
	LOGD(LOG_JNI, "Starting Loop");
	// For each stroke
	for (int s = 0; s < numStrokes; s++) {
		// Get the stroke
		jobject stroke = env->GetObjectArrayElement(strokeList, s);
		LTKTrace trace;

		int numPoints = env->CallIntMethod(stroke, getNumPointsMethodID);
		LOGD(LOG_JNI, "Got number of points: %d", numPoints);
		for (int p = 0; p < numPoints; p++) {
			jobject point = env->CallObjectMethod(stroke, getPointsAtMethodID, p);
			pointVec.push_back(env->GetFloatField(point, xFieldID));
			pointVec.push_back(env->GetFloatField(point, yFieldID));
			LOGD(LOG_JNI, "X and Y are: %f, %f", numPoints);
			trace.addPoint(pointVec);
			pointVec.clear();
		}
		LOGD(LOG_JNI, "Adding trace");
		traceGroup.addTrace(trace);
		
	}

	LOGD(LOG_JNI, "Recognizing...");

	// RECOGNIZE THIS
	int iResult = gShapeReco->recognize(traceGroup, screenContext, outSubSetOfClasses, CONFIDENCE_THRESHOLD, NUMBER_OF_RESULTS, outResults);

	LOGD(LOG_JNI, "Recognized...");

	// Build an object array to return
	jclass resultClass = env->FindClass("rcos/main/recognition/LipitkResult");

     	jfieldID id = env->GetFieldID(resultClass, "Id", "I");
    	jfieldID confidence = env->GetFieldID(resultClass, "Confidence", "F");
			
	jobjectArray resultSetArray = env->NewObjectArray(outResults.size(), resultClass, NULL);
	jmethodID constructorMethodID = env->GetMethodID(resultClass, "<init>", "()V");
		  
	for (int k = 0; k < outResults.size(); k++) {
		jobject obj = env->NewObject(resultClass, constructorMethodID);
		env->SetIntField(obj, id, outResults[k].getShapeId());
		env->SetFloatField(obj, confidence, outResults[k].getConfidence());
		env->SetObjectArrayElement(resultSetArray, k, obj);
		obj = NULL;
	}

	return resultSetArray;
}
