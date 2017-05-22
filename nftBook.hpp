#ifndef SEVER_NFT_BOOK_HPP
#define SEVER_NFT_BOOK_HPP

// ============================================================================
//     Includes
// ============================================================================

#include <stdlib.h> // malloc()

#include <AR/ar.h>
#include <AR/arMulti.h>
#include <AR/video.h>
#include <AR/gsub_es.h>
#include <AR/arFilterTransMat.h>
#include <AR2/tracking.h>
#include <AR/arosg.h>

#include "ARMarkerNFT.h"
#include "trackingSub.h"
#include "VirtualEnvironment.h"
#include "osgPlugins.h"

// ============================================================================
//     Types
// ============================================================================

typedef enum {
  ARViewContentModeScaleToFill,
  ARViewContentModeScaleAspectFit,      // contents scaled to fit with fixed aspect. remainder is transparent
  ARViewContentModeScaleAspectFill,     // contents scaled to fill with fixed aspect. some portion of content may be clipped.
  //ARViewContentModeRedraw,              // redraw on bounds change
  ARViewContentModeCenter,              // contents remain same size. positioned adjusted.
  ARViewContentModeTop,
  ARViewContentModeBottom,
  ARViewContentModeLeft,
  ARViewContentModeRight,
  ARViewContentModeTopLeft,
  ARViewContentModeTopRight,
  ARViewContentModeBottomLeft,
  ARViewContentModeBottomRight,
} ARViewContentMode;

enum viewPortIndices {
  viewPortIndexLeft = 0,
  viewPortIndexBottom,
  viewPortIndexWidth,
  viewPortIndexHeight
};

// ============================================================================
//     Constants
// ============================================================================

// Maximum number of pages expected.
// You can change this down (to save memory) or up (to accomodate more pages.)
#define PAGES_MAX 10

#ifndef MAX
#  define MAX(X,Y) ((X) > (Y) ? (X) : (Y))
#endif
#ifndef MIN
#  define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif

// ============================================================================
//     Global variables
// ============================================================================

// Preferences.
static const char *cparaName = "Data/camera_para.dat"; ///< Camera parameters file
static const char *markerConfigDataFilename = "Data/markers.dat";
static const char *objectDataFilename = "Data/objects.dat";

// Image acquisition.
static AR2VideoParamT *gVid = NULL;
static bool videoInited = false; ///< true when ready to receive video frames.
static int videoWidth = 0; ///< Width of the video frame in pixels.
static int videoHeight = 0; ///< Height of the video frame in pixels.
static AR_PIXEL_FORMAT gPixFormat; ///< Pixel format from ARToolKit enumeration.
static ARUint8* gVideoFrame = NULL; ///< Buffer containing current video frame.
static size_t gVideoFrameSize = 0; ///< Size of buffer containing current video frame.
static bool videoFrameNeedsPixelBufferDataUpload = false;
static int gCameraIndex = 0;
static bool gCameraIsFrontFacing = false;

// Markers.
static ARMarkerNFT *markersNFT = NULL;
static int markersNFTCount = 0;

// NFT.
static THREAD_HANDLE_T     *trackingThreadHandle = NULL;
static AR2HandleT          *ar2Handle = NULL;
static KpmHandle           *kpmHandle = NULL;
static int                  surfaceSetCount = 0;
static AR2SurfaceSetT      *surfaceSet[PAGES_MAX];
static THREAD_HANDLE_T     *nftDataLoadingThreadHandle = NULL;
static int                  nftDataLoaded = false;

// NFT results.
static int detectedPage = -2; // -2 Tracking not inited, -1 tracking inited OK, >= 0 tracking online on page.
static float trackingTrans[3][4];

// Drawing.
static int backingWidth;
static int backingHeight;
static GLint viewPort[4];
static ARViewContentMode gContentMode = ARViewContentModeScaleAspectFill;
static bool gARViewLayoutRequired = false;
static ARParamLT *gCparamLT = NULL; ///< Camera paramaters
static ARGL_CONTEXT_SETTINGS_REF gArglSettings = NULL; ///< GL settings for rendering video background
static const ARdouble NEAR_PLANE = 10.0f; ///< Near plane distance for projection matrix calculation
static const ARdouble FAR_PLANE = 5000.0f; ///< Far plane distance for projection matrix calculation
static ARdouble cameraLens[16];
static ARdouble cameraPose[16];
static int cameraPoseValid;
static bool gARViewInited = false;

// Drawing orientation.
static int gDisplayOrientation = 1; // range [0-3]. 1=landscape.
static int gDisplayWidth = 0;
static int gDisplayHeight = 0;
static int gDisplayDPI = 160; // Android default.

static bool gContentRotate90 = false;
static bool gContentFlipV = false;
static bool gContentFlipH = false;

// Network.
static int gInternetState = -1;

// ============================================================================
//     Functions
// ============================================================================

void
nftBookInit();

void
nftBookStart();

void
nftBookStop();

void
nftBookDestroy();

// need meta parameter: width, height, Android cameraIndex, CameraIsFrontFacing
void
videoInit(int w, int h, int cameraIndex, bool cameraIsFrontFacing);

void
processVedieoFrame(size_t size, ARUint8* frameBuf);

#endif // SEVER_NFT_BOOK_HPP
