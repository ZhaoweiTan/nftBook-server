#include "nftBook.hpp"

void
nftBookInit()
{
  // Load marker(s).
  newMarkers(markerConfigDataFilename, &markersNFT, &markersNFTCount);
  if (!markersNFTCount) {
    std::cerr << "Error loading markers from config file" << markerConfigDataFilename << std::endl;
  }
}

void
nftBookStart()
{
  gVid = ar2VideoOpen("");
  if (!gVid) {
    std::cerr << "Error calling video open" << std::endl;
  }

  // Since most NFT init can't be completed until the video frame size is known,
  // and NFT surface loading depends on NFT init, all that will be deferred.

  // Also, VirtualEnvironment init depends on having an OpenGL context, and so that also
  // forces us to defer VirtualEnvironment init.

  // ARGL init depends on both these things, which forces us to defer it until the
  // main frame loop.
}

void
nftBookStop()
{
  int i, j;

  // Can't call arglCleanup() or VirtualEnvironmentFinal() here, because nativeStop is not called on rendering thread.

  // NFT cleanup.
  if (trackingThreadHandle) {
    trackingInitQuit(&trackingThreadHandle);
    detectedPage = -2;
  }
  j = 0;
  for (i = 0; i < surfaceSetCount; i++) {
    if (surfaceSet[i]) {
      ar2FreeSurfaceSet(&surfaceSet[i]); // Sets surfaceSet[i] to NULL.
      j++;
    }
  }
  surfaceSetCount = 0;
  nftDataLoaded = false;
  ar2DeleteHandle(&ar2Handle);
  kpmDeleteHandle(&kpmHandle);
  arParamLTFree(&gCparamLT);

  // OpenGL cleanup -- not done here.

  // Video cleanup.
  if (gVideoFrame) {
    free(gVideoFrame);
    gVideoFrame = NULL;
    gVideoFrameSize = 0;
  }
  ar2VideoClose(gVid);
  gVid = NULL;
  videoInited = false;
}

void
nftBookDestroy()
{
  if (markersNFT)
    deleteMarkers(&markersNFT, &markersNFTCount);
}

void
videoInit(int w, int h, int cameraIndex, bool cameraIsFrontFacing)
{
  // As of ARToolKit v5.0, NV21 format video frames are handled natively,
  // and no longer require colour conversion to RGBA. A buffer (gVideoFrame)
  // must be set aside to copy the frame from the Java side.
  // If you still require RGBA format information from the video,
  // you can create your own additional buffer, and then unpack the NV21
  // frames into it in nativeVideoFrame() below.
  // Here is where you'd allocate the buffer:
  // ARUint8 *myRGBABuffer = (ARUint8 *)malloc(videoWidth * videoHeight * 4);
  gPixFormat = AR_PIXEL_FORMAT_NV21;
  gVideoFrameSize = (sizeof(ARUint8)*(w*h + 2*w/2*h/2));
  gVideoFrame = (ARUint8*) (malloc(gVideoFrameSize));
  if (!gVideoFrame) {
    gVideoFrameSize = 0;
    std::cerr << "Error allocating frame buffer" << std::endl;
    return
  }
  videoWidth = w;
  videoHeight = h;
  gCameraIndex = cameraIndex;
  gCameraIsFrontFacing = cameraIsFrontFacing;

  ar2VideoSetParami(gVid, AR_VIDEO_PARAM_ANDROID_WIDTH, videoWidth);
  ar2VideoSetParami(gVid, AR_VIDEO_PARAM_ANDROID_HEIGHT, videoHeight);
  ar2VideoSetParami(gVid, AR_VIDEO_PARAM_ANDROID_PIXELFORMAT, (int)gPixFormat);
  ar2VideoSetParami(gVid, AR_VIDEO_PARAM_ANDROID_CAMERA_INDEX, gCameraIndex);
  ar2VideoSetParami(gVid, AR_VIDEO_PARAM_ANDROID_CAMERA_FACE, gCameraIsFrontFacing);
  ar2VideoSetParami(gVid, AR_VIDEO_PARAM_ANDROID_INTERNET_STATE, gInternetState);

  if (ar2VideoGetCParamAsync(gVid, nativeVideoGetCparamCallback, NULL) < 0) {
    std::cerr << "Error getting cparam.\n" << std::endl;
    nativeVideoGetCparamCallback(NULL, NULL);
  }
}

void
processVedieoFrame(size_t size, ARUint8* frameBuf)
{
  gVideoFrameSize = size;
  gVideoFrame = frameBuf;

  // As of ARToolKit v5.0, NV21 format video frames are handled natively,
  // and no longer require colour conversion to RGBA.
  // If you still require RGBA format information from the video,
  // here is where you'd do the conversion:
  // color_convert_common(gVideoFrame, gVideoFrame + videoWidth*videoHeight, videoWidth, videoHeight, myRGBABuffer);

  videoFrameNeedsPixelBufferDataUpload = true; // Note that buffer needs uploading. (Upload must be done on OpenGL context's thread.)

  // Run marker detection on frame
  if (trackingThreadHandle) {
    // Perform NFT tracking.
    float            err;
    int              ret;
    int              pageNo;

    if( detectedPage == -2 ) {
      trackingInitStart( trackingThreadHandle, gVideoFrame );
      detectedPage = -1;
    }
    if( detectedPage == -1 ) {
      ret = trackingInitGetResult( trackingThreadHandle, trackingTrans, &pageNo);
      if( ret == 1 ) {
        if (pageNo >= 0 && pageNo < surfaceSetCount) {
          detectedPage = pageNo;
          ar2SetInitTrans(surfaceSet[detectedPage], trackingTrans);
        } else {
          LOGE("Detected bad page %d.\n", pageNo);
          detectedPage = -2;
        }
      } else if( ret < 0 ) {
        detectedPage = -2;
      }
    }
    if( detectedPage >= 0 && detectedPage < surfaceSetCount) {
      if( ar2Tracking(ar2Handle, surfaceSet[detectedPage], gVideoFrame, trackingTrans, &err) < 0 ) {
        detectedPage = -2;
      } else {
      }
    }
  } else {
    LOGE("Error: trackingThreadHandle\n");
    detectedPage = -2;
  }

  // Update markers.
  for (i = 0; i < markersNFTCount; i++) {
    markersNFT[i].validPrev = markersNFT[i].valid;
    if (markersNFT[i].pageNo >= 0 && markersNFT[i].pageNo == detectedPage) {
      markersNFT[i].valid = TRUE;
      for (j = 0; j < 3; j++)
        for (k = 0; k < 4; k++)
          markersNFT[i].trans[j][k] = trackingTrans[j][k];
    }
    else markersNFT[i].valid = FALSE;
    if (markersNFT[i].valid) {

      // Filter the pose estimate.
      if (markersNFT[i].ftmi) {
        if (arFilterTransMat(markersNFT[i].ftmi, markersNFT[i].trans, !markersNFT[i].validPrev) < 0) {
          LOGE("arFilterTransMat error with marker %d.\n", i);
        }
      }

      if (!markersNFT[i].validPrev) {
        // Marker has become visible, tell any dependent objects.
        VirtualEnvironmentHandleARMarkerAppeared(i);
      }

      // We have a new pose, so set that.
      arglCameraViewRHf(markersNFT[i].trans, markersNFT[i].pose.T, 1.0f /*VIEW_SCALEFACTOR*/);
      // Tell any dependent objects about the update.
      VirtualEnvironmentHandleARMarkerWasUpdated(i, markersNFT[i].pose);

    } else {

      if (markersNFT[i].validPrev) {
        // Marker has ceased to be visible, tell any dependent objects.
        VirtualEnvironmentHandleARMarkerDisappeared(i);
      }
    }
  }
}

static void
nativeVideoGetCparamCallback(const ARParam *cparam_p, void *userdata)
{
  // Load the camera parameters, resize for the window and init.
  ARParam cparam;
  if (cparam_p) cparam = *cparam_p;
  else {
    std::cerr << "Unable to automatically determine camera parameters. Using default" << std::endl;
    if (arParamLoad(cparaName, 1, &cparam) < 0) {
      std::cerr << "Error: Unable to load parameter file" << cparaName << " for camera" << std::endl;
      return;
    }
  }
  if (cparam.xsize != videoWidth || cparam.ysize != videoHeight) {
    arParamChangeSize(&cparam, videoWidth, videoHeight, &cparam);
  }
  if ((gCparamLT = arParamLTCreate(&cparam, AR_PARAM_LT_DEFAULT_OFFSET)) == NULL) {
    std::cerr << "Error: arParamLTCreate" << std::endl;
    return;
  }
  videoInited = true;

  //
  // AR init.
  //

  // Create the OpenGL projection from the calibrated camera parameters.
  arglCameraFrustumRHf(&gCparamLT->param, NEAR_PLANE, FAR_PLANE, cameraLens);
  cameraPoseValid = FALSE;

  if (!initNFT(gCparamLT, gPixFormat)) {
    std::cerr << "Error initialising NFT" << std::endl;
    arParamLTFree(&gCparamLT);
    return;
  }

  // Marker data has already been loaded, so now load NFT data on a second thread.
  nftDataLoadingThreadHandle = threadInit(0, NULL, loadNFTDataAsync);
  if (!nftDataLoadingThreadHandle) {
    std::cerr << "Error starting NFT loading thread" << std::endl;
    arParamLTFree(&gCparamLT);
    return;
  }
  threadStartSignal(nftDataLoadingThreadHandle);
}

static bool
initNFT(ARParamLT *cparamLT, AR_PIXEL_FORMAT pixFormat)
{
  //
  // NFT init.
  //

  // KPM init.
  kpmHandle = kpmCreateHandle(cparamLT, pixFormat);
  if (!kpmHandle) {
    std::cerr << "Error: kpmCreatHandle" << std::endl;
    return false;
  }
  //kpmSetProcMode( kpmHandle, KpmProcHalfSize );

  // AR2 init.
  if( (ar2Handle = ar2CreateHandle(cparamLT, pixFormat, AR2_TRACKING_DEFAULT_THREAD_NUM)) == NULL ) {
    std::cerr << "Error: ar2CreateHandle" << std::endl;
    kpmDeleteHandle(&kpmHandle);
    return (false);
  }
  if (threadGetCPU() <= 1) {
    ar2SetTrackingThresh( ar2Handle, 5.0 );
    ar2SetSimThresh( ar2Handle, 0.50 );
    ar2SetSearchFeatureNum(ar2Handle, 16);
    ar2SetSearchSize(ar2Handle, 6);
    ar2SetTemplateSize1(ar2Handle, 6);
    ar2SetTemplateSize2(ar2Handle, 6);
  } else {
    ar2SetTrackingThresh( ar2Handle, 5.0 );
    ar2SetSimThresh( ar2Handle, 0.50 );
    ar2SetSearchFeatureNum(ar2Handle, 16);
    ar2SetSearchSize(ar2Handle, 12);
    ar2SetTemplateSize1(ar2Handle, 6);
    ar2SetTemplateSize2(ar2Handle, 6);
  }
  // NFT dataset loading will happen later.
  return true;
}

static void*
loadNFTDataAsync(THREAD_HANDLE_T *threadHandle)
{
  int i, j;
  KpmRefDataSet *refDataSet;

  while (threadStartWait(threadHandle) == 0) {

    // If data was already loaded, stop KPM tracking thread and unload previously loaded data.
    if (trackingThreadHandle) {
      std::cerr << "NFT2 tracking thread is running. Stopping it first." << std::endl;
      trackingInitQuit(&trackingThreadHandle);
      detectedPage = -2;
    }
    j = 0;
    for (i = 0; i < surfaceSetCount; i++) {
      if (j == 0)
        std::cerr << "Unloading NFT tracking surfaces.");
      ar2FreeSurfaceSet(&surfaceSet[i]); // Also sets surfaceSet[i] to NULL.
      j++;
    }
    if (j > 0)
      std::cerr << "Unloaded " << j << " NFT tracking surfaces" << std::endl;
    surfaceSetCount = 0;

    refDataSet = NULL;

    for (i = 0; i < markersNFTCount; i++) {
      // Load KPM data.
      KpmRefDataSet  *refDataSet2;
      if (kpmLoadRefDataSet(markersNFT[i].datasetPathname, "fset3", &refDataSet2) < 0 ) {
        std::cerr << "Error reading KPM data from " << markersNFT[i].datasetPathname
                  <<" fset3\n";
        markersNFT[i].pageNo = -1;
        continue;
      }
      markersNFT[i].pageNo = surfaceSetCount;
      std::cerr << "  Assigned page no. %d.\n", surfaceSetCount);
      if (kpmChangePageNoOfRefDataSet(refDataSet2, KpmChangePageNoAllPages, surfaceSetCount) < 0) {
        std::cerr << "Error: kpmChangePageNoOfRefDataSet" << std::endl;
        exit(-1);
      }
      if (kpmMergeRefDataSet(&refDataSet, &refDataSet2) < 0) {
        std::cerr << "Error: kpmMergeRefDataSet" << std::endl;
        exit(-1);
      }

      // Load AR2 data.
      if ((surfaceSet[surfaceSetCount] = ar2ReadSurfaceSet(markersNFT[i].datasetPathname, "fset", NULL)) == NULL ) {
        std::cerr << "Error reading data from %s.fset\n", markersNFT[i].datasetPathname);
      }

      surfaceSetCount++;
      if (surfaceSetCount == PAGES_MAX) break;
    }
    if (kpmSetRefDataSet(kpmHandle, refDataSet) < 0) {
      std::cerr << "Error: kpmSetRefDataSet");
      exit(-1);
    }
    kpmDeleteRefDataSet(&refDataSet);

    // Start the KPM tracking thread.
    trackingThreadHandle = trackingInitInit(kpmHandle);
    if (!trackingThreadHandle) exit(-1);

    threadEndSignal(threadHandle); // Signal that we're done.
  }
  return (NULL); // Exit this thread.
}
