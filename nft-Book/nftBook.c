// ============================================================================
//  Includes
// ============================================================================

#include <stdio.h>
#include <stdlib.h>         // malloc(), free()
#include <string.h>
#ifdef __APPLE__
#  include <GLUT/glut.h>
#else
#  include <GL/glut.h>
#endif

#include <AR/ar.h>
#include <AR/arMulti.h>
#include <AR/video.h>
#include <AR/gsub_lite.h>
#include <AR2/tracking.h>

#include "ARMarkerNFT.h"
#include "trackingSub.h"
#include "VirtualEnvironment.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// ============================================================================
//  Constants
// ============================================================================

#define PAGES_MAX               10          // Maximum number of pages expected. You can change this down (to save memory) or up (to accomodate more pages.)

#define VIEW_SCALEFACTOR    1.0     // Units received from ARToolKit tracking will be multiplied by this factor before being used in OpenGL drawing.
#define VIEW_DISTANCE_MIN   10.0    // Objects closer to the camera than this will not be displayed. OpenGL units.
#define VIEW_DISTANCE_MAX   10000.0   // Objects further away from the camera than this will not be displayed. OpenGL units.

#define FONT_SIZE 10.0f
#define FONT_LINE_SPACING 2.0f

// ============================================================================
//  Global variables
// ============================================================================

// Preferences.
static int prefWindowed = TRUE;           // Use windowed (TRUE) or fullscreen mode (FALSE) on launch.
static int prefWidth = 320;               // Preferred initial window width.
static int prefHeight = 240;              // Preferred initial window height.
static int prefDepth = 16;                // Fullscreen mode bit depth. Set to 0 to use default depth.
static int prefRefresh = 0;         // Fullscreen mode refresh rate. Set to 0 to use default rate.

// Image acquisition.
static ARUint8    *gARTImage = NULL;
static long     gCallCountMarkerDetect = 0;

// Markers.
static ARMarkerNFT *markersNFT = NULL;
static int markersNFTCount = 0;

// NFT.
static THREAD_HANDLE_T     *threadHandle = NULL;
static AR2HandleT          *ar2Handle = NULL;
static KpmHandle           *kpmHandle = NULL;
static int                  surfaceSetCount = 0;
static AR2SurfaceSetT      *surfaceSet[PAGES_MAX];

// NFT results.
static int detectedPage = -2; // -2 Tracking not inited, -1 tracking inited OK, >= 0 tracking online on page.
static float trackingTrans[3][4];

// Drawing.
static int gWindowW;
static int gWindowH;
static ARParamLT *gCparamLT = NULL;
static ARGL_CONTEXT_SETTINGS_REF gArglSettings = NULL;
static double gFPS;
static ARdouble cameraLens[16];
static ARdouble cameraPose[16];
static int cameraPoseValid;

// sender client socket
struct sockaddr_in myaddr;  /* our address */
struct sockaddr_in remaddr; /* remote address */
struct sockaddr_in dstaddr;
socklen_t addrlen = sizeof(remaddr);    /* length of addresses */
int recvlen;      /* # bytes received */
int fd;       /* our socket */
int send_fd; /* sender socket */
int msgcnt = 0;     /* count # of messages we received */
int SERVICE_PORT = 10000;
int CLIENT_PORT = 9999;
int BUFSIZE = 2000;
int frame_buffer_size = 400000;


// ============================================================================
//  Function prototypes
// ============================================================================

static void usage(char *com);
static int setupCamera(const char *cparam_name, char *vconf, ARParamLT **cparamLT_p);
static int initNFT(ARParamLT *cparamLT, AR_PIXEL_FORMAT pixFormat);
static int loadNFTData(void);
static void cleanup(void);
static void Keyboard(unsigned char key, int x, int y);
static void Visibility(int visible);
static void Reshape(int w, int h);
static void Display(void);

// ============================================================================
//  Functions
// ============================================================================

int main(int argc, char** argv)
{
    
    /* receiving UDP packets from sender client */
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return 0;
    }
    
    if ((send_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("cannot create socket\n");
        return 0;
    }
    
    /* bind the socket to any valid IP address and a specific port */
    memset((char *)&myaddr, 0, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(SERVICE_PORT);
    
    memset((char *)&dstaddr, 0, sizeof(dstaddr));
    dstaddr.sin_family = AF_INET;
    dstaddr.sin_addr.s_addr = inet_addr("192.168.0.27");
    dstaddr.sin_port = htons(10001);
    
    
    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
        perror("bind failed");
        return 0;
    }
    
    char    glutGamemode[32] = "";
    char   *vconf = "-device=Dummy -width=320 -height=240 -format=RGBA";
    char    cparaDefault[] = "Data2/camera_para.dat";
    char   *cpara = NULL;
    int     i;
    int     gotTwoPartOption;
    const char markerConfigDataFilename[] = "Data2/markers.dat";
    const char objectDataFilename[] = "Data2/objects.dat";
    
#ifdef DEBUG
    arLogLevel = AR_LOG_LEVEL_DEBUG;
#endif
    
    //
    // Process command-line options.
    //
    
    glutInit(&argc, argv);
    
    i = 1; // argv[0] is name of app, so start at 1.
    while (i < argc) {
        gotTwoPartOption = FALSE;
        // Look for two-part options first.
        if ((i + 1) < argc) {
            if (strcmp(argv[i], "--vconf") == 0) {
                i++;
                vconf = argv[i];
                gotTwoPartOption = TRUE;
            } else if (strcmp(argv[i], "--cpara") == 0) {
                i++;
                cpara = argv[i];
                gotTwoPartOption = TRUE;
            } else if (strcmp(argv[i],"--width") == 0) {
                i++;
                // Get width from second field.
                if (sscanf(argv[i], "%d", &prefWidth) != 1) {
                    ARLOGe("Error: --width option must be followed by desired width.\n");
                }
                gotTwoPartOption = TRUE;
            } else if (strcmp(argv[i],"--height") == 0) {
                i++;
                // Get height from second field.
                if (sscanf(argv[i], "%d", &prefHeight) != 1) {
                    ARLOGe("Error: --height option must be followed by desired height.\n");
                }
                gotTwoPartOption = TRUE;
            } else if (strcmp(argv[i],"--refresh") == 0) {
                i++;
                // Get refresh rate from second field.
                if (sscanf(argv[i], "%d", &prefRefresh) != 1) {
                    ARLOGe("Error: --refresh option must be followed by desired refresh rate.\n");
                }
                gotTwoPartOption = TRUE;
            }
        }
        if (!gotTwoPartOption) {
            // Look for single-part options.
            if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "-h") == 0) {
                usage(argv[0]);
            } else if (strncmp(argv[i], "-cpara=", 7) == 0) {
                cpara = &(argv[i][7]);
            } else if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-version") == 0 || strcmp(argv[i], "-v") == 0) {
                ARLOG("%s version %s\n", argv[0], AR_HEADER_VERSION_STRING);
                exit(0);
            } else if (strcmp(argv[i],"--windowed") == 0) {
                prefWindowed = TRUE;
            } else if (strcmp(argv[i],"--fullscreen") == 0) {
                prefWindowed = FALSE;
            } else {
                ARLOGe("Error: invalid command line argument '%s'.\n", argv[i]);
                usage(argv[0]);
            }
        }
        i++;
    }
    
    
    //
    // Video setup.
    //
    
    if (!setupCamera((cpara ? cpara : cparaDefault), vconf, &gCparamLT)) {
        ARLOGe("main(): Unable to set up AR camera.\n");
        exit(-1);
    }
    
    //
    // AR init.
    //
    
    if (!initNFT(gCparamLT, arVideoGetPixelFormat())) {
        ARLOGe("main(): Unable to init NFT.\n");
        exit(-1);
    }
    
    //
    // Markers setup.
    //
    
    // Load marker(s).
    newMarkers(markerConfigDataFilename, &markersNFT, &markersNFTCount);
    if (!markersNFTCount) {
        ARLOGe("Error loading markers from config. file '%s'.\n", markerConfigDataFilename);
        cleanup();
        exit(-1);
    }
    ARLOGi("Marker count = %d\n", markersNFTCount);
    
    // Marker data has been loaded, so now load NFT data.
    if (!loadNFTData()) {
        ARLOGe("Error loading NFT data.\n");
        cleanup();
        exit(-1);
    }
    
    //
    // Graphics setup.
    //
    
    // Set up GL context(s) for OpenGL to draw into.
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    if (prefWindowed) {
        if (prefWidth > 0 && prefHeight > 0) glutInitWindowSize(prefWidth, prefHeight);
        else glutInitWindowSize(gCparamLT->param.xsize, gCparamLT->param.ysize);
        glutCreateWindow(argv[0]);
    } else {
        if (glutGameModeGet(GLUT_GAME_MODE_POSSIBLE)) {
            if (prefWidth && prefHeight) {
                if (prefDepth) {
                    if (prefRefresh) snprintf(glutGamemode, sizeof(glutGamemode), "%ix%i:%i@%i", prefWidth, prefHeight, prefDepth, prefRefresh);
                    else snprintf(glutGamemode, sizeof(glutGamemode), "%ix%i:%i", prefWidth, prefHeight, prefDepth);
                } else {
                    if (prefRefresh) snprintf(glutGamemode, sizeof(glutGamemode), "%ix%i@%i", prefWidth, prefHeight, prefRefresh);
                    else snprintf(glutGamemode, sizeof(glutGamemode), "%ix%i", prefWidth, prefHeight);
                }
            } else {
                prefWidth = glutGameModeGet(GLUT_GAME_MODE_WIDTH);
                prefHeight = glutGameModeGet(GLUT_GAME_MODE_HEIGHT);
                snprintf(glutGamemode, sizeof(glutGamemode), "%ix%i", prefWidth, prefHeight);
            }
            glutGameModeString(glutGamemode);
            glutEnterGameMode();
        } else {
            if (prefWidth > 0 && prefHeight > 0) glutInitWindowSize(prefWidth, prefHeight);
            glutCreateWindow(argv[0]);
            glutFullScreen();
        }
    }
    
    // Create the OpenGL projection from the calibrated camera parameters.
    arglCameraFrustumRH(&(gCparamLT->param), VIEW_DISTANCE_MIN, VIEW_DISTANCE_MAX, cameraLens);
    cameraPoseValid = FALSE;
    
    // Setup ARgsub_lite library for current OpenGL context.
    if ((gArglSettings = arglSetupForCurrentContext(&(gCparamLT->param), arVideoGetPixelFormat())) == NULL) {
        ARLOGe("main(): arglSetupForCurrentContext() returned error.\n");
        cleanup();
        exit(-1);
    }
    
    // Load objects (i.e. OSG models).
    VirtualEnvironmentInit(objectDataFilename);
    VirtualEnvironmentHandleARViewUpdatedCameraLens(cameraLens);
    
    //
    // Setup complete. Start tracking.
    //
    
    // Start the video.
    if (arVideoCapStart() != 0) {
        ARLOGe("setupCamera(): Unable to begin camera data capture.\n");
        return (FALSE);
    }
    arUtilTimerReset();
    
    // Register GLUT event-handling callbacks.
    // NB: mainLoop() is registered by Visibility.
    glutDisplayFunc(Display);
    glutReshapeFunc(Reshape);
    glutVisibilityFunc(Visibility);
    glutKeyboardFunc(Keyboard);
    
    glutMainLoop();
    
    return (0);
}

static void usage(char *com)
{
    ARLOG("Usage: %s [options]\n", com);
    ARLOG("Options:\n");
    ARLOG("  --vconf <video parameter for the camera>\n");
    ARLOG("  --cpara <camera parameter file for the camera>\n");
    ARLOG("  -cpara=<camera parameter file for the camera>\n");
    ARLOG("  --width w     Use display/window width of w pixels.\n");
    ARLOG("  --height h    Use display/window height of h pixels.\n");
    ARLOG("  --refresh f   Use display refresh rate of f Hz.\n");
    ARLOG("  --windowed    Display in window, rather than fullscreen.\n");
    ARLOG("  --fullscreen  Display fullscreen, rather than in window.\n");
    ARLOG("  -h -help --help: show this message\n");
    exit(0);
}

static int setupCamera(const char *cparam_name, char *vconf, ARParamLT **cparamLT_p)
{
    ARParam     cparam;
    int       xsize, ysize;
    AR_PIXEL_FORMAT pixFormat;
    
    // Open the video path.
    if (arVideoOpen(vconf) < 0) {
        ARLOGe("setupCamera(): Unable to open connection to camera.\n");
        return (FALSE);
    }
    
    // Find the size of the window.
    if (arVideoGetSize(&xsize, &ysize) < 0) {
        ARLOGe("setupCamera(): Unable to determine camera frame size.\n");
        arVideoClose();
        return (FALSE);
    }
    
    ARLOGi("Camera image size (x,y) = (%d,%d)\n", xsize, ysize);
    
    // Get the format in which the camera is returning pixels.
    pixFormat = arVideoGetPixelFormat();
    
    if (pixFormat == AR_PIXEL_FORMAT_INVALID) {
        ARLOGe("setupCamera(): Camera is using unsupported pixel format.\n");
        arVideoClose();
        return (FALSE);
    }
    
    // Load the camera parameters, resize for the window and init.
    if (arParamLoad(cparam_name, 1, &cparam) < 0) {
        ARLOGe("setupCamera(): Error loading parameter file %s for camera.\n", cparam_name);
        arVideoClose();
        return (FALSE);
    }
    if (cparam.xsize != xsize || cparam.ysize != ysize) {
        ARLOGw("*** Camera Parameter resized from %d, %d. ***\n", cparam.xsize, cparam.ysize);
        arParamChangeSize(&cparam, xsize, ysize, &cparam);
    }
#ifdef DEBUG
    ARLOG("*** Camera Parameter ***\n");
    arParamDisp(&cparam);
#endif
    if ((*cparamLT_p = arParamLTCreate(&cparam, AR_PARAM_LT_DEFAULT_OFFSET)) == NULL) {
        ARLOGe("setupCamera(): Error: arParamLTCreate.\n");
        arVideoClose();
        return (FALSE);
    }
    
    return (TRUE);
}

// Modifies globals: kpmHandle, ar2Handle.
static int initNFT(ARParamLT *cparamLT, AR_PIXEL_FORMAT pixFormat)
{
    ARLOGd("Initialising NFT.\n");
    //
    // NFT init.
    //
    
    // KPM init.
    kpmHandle = kpmCreateHandle(cparamLT, pixFormat);
    if (!kpmHandle) {
        ARLOGe("Error: kpmCreateHandle.\n");
        return (FALSE);
    }
    //kpmSetProcMode( kpmHandle, KpmProcHalfSize );
    
    // AR2 init.
    if( (ar2Handle = ar2CreateHandle(cparamLT, pixFormat, AR2_TRACKING_DEFAULT_THREAD_NUM)) == NULL ) {
        ARLOGe("Error: ar2CreateHandle.\n");
        kpmDeleteHandle(&kpmHandle);
        return (FALSE);
    }
    if (threadGetCPU() <= 1) {
        ARLOGi("Using NFT tracking settings for a single CPU.\n");
        ar2SetTrackingThresh(ar2Handle, 5.0);
        ar2SetSimThresh(ar2Handle, 0.50);
        ar2SetSearchFeatureNum(ar2Handle, 16);
        ar2SetSearchSize(ar2Handle, 6);
        ar2SetTemplateSize1(ar2Handle, 6);
        ar2SetTemplateSize2(ar2Handle, 6);
    } else {
        ARLOGi("Using NFT tracking settings for more than one CPU.\n");
        ar2SetTrackingThresh(ar2Handle, 5.0);
        ar2SetSimThresh(ar2Handle, 0.50);
        ar2SetSearchFeatureNum(ar2Handle, 16);
        ar2SetSearchSize(ar2Handle, 12);
        ar2SetTemplateSize1(ar2Handle, 6);
        ar2SetTemplateSize2(ar2Handle, 6);
    }
    // NFT dataset loading will happen later.
    return (TRUE);
}

// Modifies globals: threadHandle, surfaceSet[], surfaceSetCount
static int unloadNFTData(void)
{
    int i, j;
    
    if (threadHandle) {
        ARLOGi("Stopping NFT2 tracking thread.\n");
        trackingInitQuit(&threadHandle);
    }
    j = 0;
    for (i = 0; i < surfaceSetCount; i++) {
        if (j == 0) ARLOGi("Unloading NFT tracking surfaces.\n");
        ar2FreeSurfaceSet(&surfaceSet[i]); // Also sets surfaceSet[i] to NULL.
        j++;
    }
    if (j > 0) ARLOGi("Unloaded %d NFT tracking surfaces.\n", j);
    surfaceSetCount = 0;
    
    return 0;
}

// References globals: markersNFTCount
// Modifies globals: threadHandle, surfaceSet[], surfaceSetCount, markersNFT[]
static int loadNFTData(void)
{
    int i;
    KpmRefDataSet *refDataSet;
    
    // If data was already loaded, stop KPM tracking thread and unload previously loaded data.
    if (threadHandle) {
        ARLOGi("Reloading NFT data.\n");
        unloadNFTData();
    } else {
        ARLOGi("Loading NFT data.\n");
    }
    
    refDataSet = NULL;
    
    for (i = 0; i < markersNFTCount; i++) {
        // Load KPM data.
        KpmRefDataSet  *refDataSet2;
        ARLOGi("Reading %s.fset3\n", markersNFT[i].datasetPathname);
        if (kpmLoadRefDataSet(markersNFT[i].datasetPathname, "fset3", &refDataSet2) < 0 ) {
            ARLOGe("Error reading KPM data from %s.fset3\n", markersNFT[i].datasetPathname);
            markersNFT[i].pageNo = -1;
            continue;
        }
        markersNFT[i].pageNo = surfaceSetCount;
        ARLOGi("  Assigned page no. %d.\n", surfaceSetCount);
        if (kpmChangePageNoOfRefDataSet(refDataSet2, KpmChangePageNoAllPages, surfaceSetCount) < 0) {
            ARLOGe("Error: kpmChangePageNoOfRefDataSet\n");
            exit(-1);
        }
        if (kpmMergeRefDataSet(&refDataSet, &refDataSet2) < 0) {
            ARLOGe("Error: kpmMergeRefDataSet\n");
            exit(-1);
        }
        ARLOGi("  Done.\n");
        
        // Load AR2 data.
        ARLOGi("Reading %s.fset\n", markersNFT[i].datasetPathname);
        
        if ((surfaceSet[surfaceSetCount] = ar2ReadSurfaceSet(markersNFT[i].datasetPathname, "fset", NULL)) == NULL ) {
            ARLOGe("Error reading data from %s.fset\n", markersNFT[i].datasetPathname);
        }
        ARLOGi("  Done.\n");
        
        surfaceSetCount++;
        if (surfaceSetCount == PAGES_MAX) break;
    }
    if (kpmSetRefDataSet(kpmHandle, refDataSet) < 0) {
        ARLOGe("Error: kpmSetRefDataSet\n");
        exit(-1);
    }
    kpmDeleteRefDataSet(&refDataSet);
    
    // Start the KPM tracking thread.
    threadHandle = trackingInitInit(kpmHandle);
    if (!threadHandle) exit(-1);
    
    ARLOGi("Loading of NFT data complete.\n");
    return (TRUE);
}

static void cleanup(void)
{
    VirtualEnvironmentFinal();
    
    if (markersNFT) deleteMarkers(&markersNFT, &markersNFTCount);
    
    // NFT cleanup.
    unloadNFTData();
    ARLOGd("Cleaning up ARToolKit NFT handles.\n");
    ar2DeleteHandle(&ar2Handle);
    kpmDeleteHandle(&kpmHandle);
    arParamLTFree(&gCparamLT);
    
    // OpenGL cleanup.
    arglCleanup(gArglSettings);
    gArglSettings = NULL;
    
    // Camera cleanup.
    arVideoCapStop();
    arVideoClose();
}

static void Keyboard(unsigned char key, int x, int y)
{
    switch (key) {
        case 0x1B:            // Quit.
        case 'Q':
        case 'q':
            cleanup();
            exit(0);
            break;
        case '?':
        case '/':
            ARLOG("Keys:\n");
            ARLOG(" q or [esc]    Quit demo.\n");
            ARLOG(" ? or /        Show this help.\n");
            ARLOG("\nAdditionally, the ARVideo library supplied the following help text:\n");
            arVideoDispOption();
            break;
        default:
            break;
    }
}

static void mainLoop(void)
{
    static int ms_prev;
    int ms;
    float s_elapsed;
    ARUint8 *image;
    
    
    int             i, j, k;
    
    unsigned char buf[BUFSIZE];
    ARUint8 * whole_frame = (ARUint8 *)malloc(frame_buffer_size);
    int last_id = 0;
    /* now loop, receiving data and printing what we received */
    int total_size_received = 0;
    while (last_id == 0)
    {
        //printf("waiting on port %d\n", SERVICE_PORT);
        recvlen = recvfrom(fd, buf, BUFSIZE, 0, (struct sockaddr *)&remaddr, &addrlen);
        if (recvlen > 0) {
            buf[recvlen] = '\0';
            if (total_size_received >= 307200)
                break;
            
//            memcpy(whole_frame+total_size_received, buf+4, recvlen-4);
//            total_size_received += recvlen-4;
//            
//            short* frame_id = (short*)malloc(sizeof(short));
//            *frame_id = 0;
//            memcpy(frame_id, buf, 2);
//            short* segment_id = (short*)malloc(sizeof(short));
//            *segment_id = 0;
//            memcpy(segment_id, buf + 2, 1);
//            short* last_segment_tag = (short*)malloc(sizeof(short));
//            *last_segment_tag = 0;
//            memcpy(last_segment_tag, buf + 3, 1);
//            last_id = *last_segment_tag;
            
            memcpy(whole_frame+total_size_received, buf+6, recvlen-6);
            total_size_received += recvlen-6;
            
            short* frame_id = (short*)malloc(sizeof(short));
            *frame_id = 0;
            memcpy(frame_id, buf, 2);
            short* segment_id = (short*)malloc(sizeof(short));
            *segment_id = 0;
            memcpy(segment_id, buf + 2, 2);
            short* last_segment_tag = (short*)malloc(sizeof(short));
            *last_segment_tag = 0;
            memcpy(last_segment_tag, buf + 4, 2);
            last_id = *last_segment_tag;

            free(frame_id);
            free(segment_id);
            free(last_segment_tag);
            //printf("received message frame id: %hi, segment id: %hi, last segment flag: %hi\n", *frame_id, *segment_id, *last_segment_tag);
        }
        else
            printf("uh oh - something went wrong!\n");
    }
    
    ARLOGe("Received a whole frame of size: %d.\n", total_size_received);
    
    // Calculate time delta.
    ms = glutGet(GLUT_ELAPSED_TIME);
    s_elapsed = (float)(ms - ms_prev) * 0.001f;
    ms_prev = ms;
    
    // Grab a video frame.
    // if ((image = arVideoGetImage()) != NULL) {
    //   gARTImage = image; // Save the fetched image.
    
    if (total_size_received == 307200) {
        
        gARTImage = whole_frame;
        
        // Calculate FPS every 30 frames.
        if (gCallCountMarkerDetect % 30 == 0) {
            gFPS = 30.0/arUtilTimer();
            arUtilTimerReset();
            gCallCountMarkerDetect = 0;
        }
        gCallCountMarkerDetect++; // Increment ARToolKit FPS counter.
        
        
        // Run marker detection on frame
        if (threadHandle) {
            // Perform NFT tracking.
            float            err;
            int              ret;
            int              pageNo;
            
            if( detectedPage == -2 ) {
                trackingInitStart( threadHandle, gARTImage );
                detectedPage = -1;
            }
            if( detectedPage == -1 ) {
                ret = trackingInitGetResult( threadHandle, trackingTrans, &pageNo);
                if( ret == 1 ) {
                    if (pageNo >= 0 && pageNo < surfaceSetCount) {
                        ARLOGd("Detected page %d.\n", pageNo);
                        detectedPage = pageNo;
                        ar2SetInitTrans(surfaceSet[detectedPage], trackingTrans);
                    } else {
                        ARLOGe("Detected bad page %d.\n", pageNo);
                        detectedPage = -2;
                    }
                } else if( ret < 0 ) {
                    ARLOGd("No page detected.\n");
                    detectedPage = -2;
                }
            }
            if( detectedPage >= 0 && detectedPage < surfaceSetCount) {
                if( ar2Tracking(ar2Handle, surfaceSet[detectedPage], gARTImage, trackingTrans, &err) < 0 ) {
                    ARLOGd("Tracking lost.\n");
                    detectedPage = -2;
                } else {
                    ARLOGd("Tracked page %d (max %d).\n", detectedPage, surfaceSetCount - 1);
                }
            }
        } else {
            ARLOGe("Error: threadHandle\n");
            detectedPage = -2;
        }
        
        int sizeofinfo = 0;
        char* marker_client_buffer = (char*)malloc(sizeof(int) + 3*4*sizeof(float) + 1);
        memcpy(marker_client_buffer, &detectedPage, sizeof(int));
        sizeofinfo += sizeof(int);
        
        for (int i = 0; i < 3; i ++)
        {
            for (int j = 0; j < 4; j ++)
            {
                memcpy(marker_client_buffer + sizeofinfo, &trackingTrans[i][j], sizeof(float));
                sizeofinfo += sizeof(float);
            }
        }

        marker_client_buffer[sizeofinfo] = '\0';
        sizeofinfo += 1;
        //char marker_client_buffer[20] = "Hello server!";

        //ARLOGe("Size of markers: %d\n", markersNFTCount);
        if (sendto(send_fd, marker_client_buffer, sizeofinfo, 0, (struct sockaddr *)&dstaddr, sizeof(dstaddr)) < 0)
        {
            ARLOGe("Sending markers to client failed.\n");
        }
        
        ARLOGe("send successully %d, %f, %f, %f\n", detectedPage, trackingTrans[0][0], trackingTrans[0][2], trackingTrans[2][0]);
        
        // Update markers.
        for (i = 0; i < markersNFTCount; i++) {
            markersNFT[i].validPrev = markersNFT[i].valid;
            if (markersNFT[i].pageNo >= 0 && markersNFT[i].pageNo == detectedPage) {
                markersNFT[i].valid = TRUE;
                for (j = 0; j < 3; j++) for (k = 0; k < 4; k++) markersNFT[i].trans[j][k] = trackingTrans[j][k];
            }
            else markersNFT[i].valid = FALSE;
            if (markersNFT[i].valid) {
                
                // Filter the pose estimate.
                if (markersNFT[i].ftmi) {
                    if (arFilterTransMat(markersNFT[i].ftmi, markersNFT[i].trans, !markersNFT[i].validPrev) < 0) {
                        ARLOGe("arFilterTransMat error with marker %d.\n", i);
                    }
                }
                
                if (!markersNFT[i].validPrev) {
                    // Marker has become visible, tell any dependent objects.
                    VirtualEnvironmentHandleARMarkerAppeared(i);
                }
                
                // We have a new pose, so set that.
                arglCameraViewRH((const ARdouble (*)[4])markersNFT[i].trans, markersNFT[i].pose.T, VIEW_SCALEFACTOR);
                // Tell any dependent objects about the update.
                VirtualEnvironmentHandleARMarkerWasUpdated(i, markersNFT[i].pose);
                
            } else {
                
                if (markersNFT[i].validPrev) {
                    // Marker has ceased to be visible, tell any dependent objects.
                    VirtualEnvironmentHandleARMarkerDisappeared(i);
                }
            }
        }
        
        
        // Tell GLUT the display has changed.
        glutPostRedisplay();
    } else {
        arUtilSleep(2);
    }
    
    free(whole_frame);
    
    
}

//
//  This function is called on events when the visibility of the
//  GLUT window changes (including when it first becomes visible).
//
static void Visibility(int visible)
{
    if (visible == GLUT_VISIBLE) {
        glutIdleFunc(mainLoop);
    } else {
        glutIdleFunc(NULL);
    }
}

//
//  This function is called when the
//  GLUT window is resized.
//
static void Reshape(int w, int h)
{
    GLint viewport[4];
    
    gWindowW = w;
    gWindowH = h;
    
    // Call through to anyone else who needs to know about window sizing here.
    viewport[0] = 0;
    viewport[1] = 0;
    viewport[2] = w;
    viewport[3] = h;
    VirtualEnvironmentHandleARViewUpdatedViewport(viewport);
}

static void print(const char *text, const float x, const float y)
{
    int i;
    size_t len;
    
    if (!text) return;
    len = strlen(text);
    glRasterPos2f(x, y);
    for (i = 0; i < len; i++) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_10, text[i]);
}

//
// This function is called when the window needs redrawing.
//
static void Display(void)
{
    char text[256];
    
    // Select correct buffer for this context.
    glDrawBuffer(GL_BACK);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the buffers for new frame.
    
    arglPixelBufferDataUpload(gArglSettings, gARTImage);
    arglDispImage(gArglSettings);
    gARTImage = NULL; // Invalidate image data.
    
    // Set up 3D mode.
    glMatrixMode(GL_PROJECTION);
#ifdef ARDOUBLE_IS_FLOAT
    glLoadMatrixf(cameraLens);
#else
    glLoadMatrixd(cameraLens);
#endif
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    
    // Set any initial per-frame GL state you require here.
    // --->
    
    // Lighting and geometry that moves with the camera should be added here.
    // (I.e. should be specified before camera pose transform.)
    // --->
    
    VirtualEnvironmentHandleARViewDrawPreCamera();
    
    if (cameraPoseValid) {
        
#ifdef ARDOUBLE_IS_FLOAT
        glMultMatrixf(cameraPose);
#else
        glMultMatrixd(cameraPose);
#endif
        
        // All lighting and geometry to be drawn in world coordinates goes here.
        // --->
        VirtualEnvironmentHandleARViewDrawPostCamera();
    }
    
    // Set up 2D mode.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, (GLdouble)gWindowW, 0, (GLdouble)gWindowH, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    
    // Add your own 2D overlays here.
    // --->
    
    VirtualEnvironmentHandleARViewDrawOverlay();
    
    // Show some real-time info.
    glColor3ub(255, 255, 255);
    // FPS.
    snprintf(text, sizeof(text), "FPS %.1f", gFPS);
    print(text, 2.0f, (float)gWindowH - 12.0f);
    
    glutSwapBuffers();
}
