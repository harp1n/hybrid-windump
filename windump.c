#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xfixes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

int format = ZPixmap;

// used for hiding the border
typedef struct
{
    unsigned long   flags;
    unsigned long   functions;
    unsigned long   decorations;
    long            inputMode;
    unsigned long   status;
} Hints;

XImage *
capture_root(Display * dpy, int screen)
{
    unsigned long swaptest = 1;
    XColor *colors;
    unsigned buffer_size;
    int win_name_size;
    int header_size;
    int ncolors, i;
    char *win_name;
    Bool got_win_name;
    XWindowAttributes win_info;
    XImage *image;
    int absx, absy, x, y;
    unsigned width, height;
    int dwidth, dheight;
    int bw;
    Window dummywin;

#if 0
    int         transparentOverlays , multiVis;
    int         numVisuals;
    XVisualInfo *pVisuals;
    int         numOverlayVisuals;
    OverlayInfo *pOverlayVisuals;
    int         numImageVisuals;
    XVisualInfo **pImageVisuals;
    list_ptr    vis_regions;    /* list of regions to read from */
    list_ptr    vis_image_regions ;
    Visual      vis_h,*vis ;
#endif
    int allImage = 0;

    Window window = RootWindow(dpy, screen);

    if (!XGetWindowAttributes(dpy, window, &win_info))
    {
        fprintf(stderr, "Can't get target window attributes.\n");
        exit(1);
    }

    absx = 0;
    absy = 0;
    win_info.x = 0;
    win_info.y = 0;
    width = win_info.width;
    height = win_info.height;
    bw = 0;

    dwidth = DisplayWidth(dpy, screen);
    dheight = DisplayHeight(dpy, screen);

    XFetchName(dpy, window, &win_name);
    if (!win_name || !win_name[0])
    {
        win_name = "xwdump";
        got_win_name = False;
    }
    else
    {
        got_win_name = True;
    }

    /* sizeof(char) is included for the null string terminator. */
    win_name_size = strlen(win_name) + sizeof(char);

    /*
     * Snarf the pixmap with XGetImage.
     */

    x = absx - win_info.x;
    y = absy - win_info.y;
#if 0
    multiVis = GetMultiVisualRegions(dpy,RootWindow(dpy, screen),absx,absy,
           width,height,&transparentOverlays,&numVisuals,&pVisuals,
               &numOverlayVisuals,&pOverlayVisuals,&numImageVisuals,
               &pImageVisuals,&vis_regions,&vis_image_regions,&allImage) ;
    if (on_root || multiVis)
    {
    if (!multiVis)
        image = XGetImage (dpy, RootWindow(dpy, screen), absx, absy,
                    width, height, AllPlanes, format);
    else
        image = ReadAreaToImage(dpy, RootWindow(dpy, screen), absx, absy,
                width, height,
                numVisuals,pVisuals,numOverlayVisuals,pOverlayVisuals,
                numImageVisuals, pImageVisuals,vis_regions,
                vis_image_regions,format,allImage);
    }
    else
#endif
    image = XGetImage(dpy, window, x, y, width, height, AllPlanes, format);
    if(!image)
    {
        fprintf(stderr, "unable to get image at %dx%d+%d+%d\n",
                width, height, x, y);
        exit(1);
    }

    //if(add_pixel_value != 0) XAddPixel (image, add_pixel_value);

    return image;
}

Window create_window(Display * dpy, int screen, int width, int height)
{
    Window win = 0;
    win = XCreateWindow(dpy, RootWindow(dpy, screen),
                        0, 0, width, height,
                        0,
                        24,
                        InputOutput,
                        DefaultVisual(dpy, screen),
                        //CWBorderPixel | CWBackPixel | CWColormap | CWEventMask | CWBitGravity,
                        0,
                        0);

    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);

    XMapWindow(dpy, win);
    return win;
}

void draw_image(Display * dpy, Window win, XImage * image)
{
    static GC gc;
    if(win != 0)
    {
        if(gc == 0)
        {
            XGCValues gc_val;
            gc = XCreateGC(dpy, win, GCForeground | GCBackground, &gc_val);
        }
        /*for(i=0;i<100;i++)
        {
            image->data*/
        XPutImage(dpy, win, gc, image, 0, 0, 0, 0, 1024, 768);
    }
}

void createShmImage(int w, int h, XShmSegmentInfo * sinfo, XShmSegmentInfo * tinfo, Display * sdpy, Display * tdpy, int sscr, int tscr, XImage ** simage, XImage ** timage)
{
    sinfo->shmid = tinfo->shmid = shmget(IPC_PRIVATE, w * h * sizeof(unsigned), IPC_CREAT | 0666);
    sinfo->shmaddr = tinfo->shmaddr = (char*) shmat(sinfo->shmid, 0, 0);
    sinfo->readOnly = tinfo->readOnly = False;
    printf("%d %d\n", DefaultDepth(sdpy, sscr), DefaultDepth(tdpy, tscr));
    *simage = XShmCreateImage(
                              sdpy, DefaultVisual(sdpy,sscr), DefaultDepth(sdpy,sscr),
                              ZPixmap/*format*/,
                              (char *)sinfo->shmaddr, sinfo,
                              w,h /*width,height*/
                             );
    *timage = XShmCreateImage(
                              tdpy, DefaultVisual(tdpy,tscr), DefaultDepth(tdpy,tscr),
                              ZPixmap/*format*/,
                              (char *)tinfo->shmaddr, tinfo,
                              w,h /*width,height*/
                             );
    XShmAttach(sdpy, sinfo);
    XShmAttach(tdpy, tinfo);
}

// Clamp a value between a lower and upper bound, inclusive.
#define CLAMP(x, l, u) ((x) < (l) ? (l) : ((x) > (u) ? (u) : (x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void drawMouse(XImage* img, XFixesCursorImage* cur, int xm, int ym)
{
    int x, y, x1, y1, x2, y2, w, h, cw, ch;

    unsigned int* data = (unsigned int*)(img->data);

    w = img->width;
    h = img->height;
    cw = cur->width;
    ch = cur->height;
    xm -= cur->xhot;
    ym -= cur->yhot;

    y = CLAMP(ym, 0, h - 1);
    x1 = MAX(-xm, 0);
    x2 = MIN(w - xm, cw);
    y1 = MAX(-ym, 0);
    y2 = MIN(h - ym, ch);
    for(y = y1; y < y2; ++y)
    {
        for(x = x1; x < x2; ++x)
        {
            // Alpha blending, ugly
            // FIXME: use the XImage RGB masks instead of hard coded colour byte order.
            unsigned long cursor_pixel = cur->pixels[y * cw + x];
            unsigned int image_pixel = data[(ym + y) * w + xm + x];

            unsigned char alpha = ((cursor_pixel & 0xFF000000) >> 24);
            unsigned char alpha_inv = 0xFF - alpha;
            unsigned char r = (((cursor_pixel & 0x00FF0000) >> 16) * alpha + ((image_pixel & 0x00FF0000) >> 16) * alpha_inv) >> 8;
            unsigned char g = (((cursor_pixel & 0x0000FF00) >> 8) * alpha + ((image_pixel & 0x0000FF00) >> 8) * alpha_inv) >> 8;
            unsigned char b = (((cursor_pixel & 0x000000FF) >> 0) * alpha + ((image_pixel & 0x000000FF) >> 0) * alpha_inv) >> 8;
            //~ unsigned char g = ((cursor_pixel & 0x0000FF00) >> 8);
            //~ unsigned char b = ((cursor_pixel & 0x000000FF) >> 0);
            //~ unsigned int color = cur->pixels[y * cw + x] * alpha + data[(ym + y) * w + xm + x] * (0xFF - alpha);
            data[(ym + y) * w + xm + x] = (r << 16) | (g << 8) | (b << 0);
        }
    }
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printf("Usage: %s source target\nwhere source and target are X11 display names.\nExample: %s :0.0 :0.1\n", argv[0], argv[0]);
        return 1;
    }
    Display * sdpy = XOpenDisplay(argv[1]);
    Display * tdpy = XOpenDisplay(argv[2]);
    int sscr = XDefaultScreen(sdpy);
    int tscr = XDefaultScreen(tdpy);
    GC tgc = DefaultGC(tdpy,tscr);
    // Source window
    Window swin = RootWindow (sdpy, sscr);
    unsigned int width, height, dummy;
    // Find dimensions of source
    XGetGeometry(sdpy, swin, (Window *)&dummy, (int *) &dummy, (int *) &dummy, &width, &height, &dummy, &dummy);
    // Create target window
    Window twin = create_window(tdpy, tscr, width, height);
    XSelectInput(sdpy, swin, PointerMotionMask);
    XImage * image;
    XImage * simage;
    XImage * timage;
    int use_shm=1;
    XShmSegmentInfo xshm_sinfo;
    XShmSegmentInfo xshm_tinfo;
    if(use_shm)
    {
        createShmImage(width, height, &xshm_sinfo, &xshm_tinfo, sdpy, tdpy, sscr, tscr, &simage, &timage);
    }
    for(;;) {
        XEvent e;
        XNextEvent(tdpy, &e);
        if (e.type == MapNotify)
            break;
    }

    int emulate_events=0;

    // begin Hide window border
    Hints   hints;
    Atom    property;
    hints.flags = 2;        // Specify that we're changing the window decorations.
    hints.decorations = 0;  // 0 (false) means that window decorations should go bye-bye.
    property = XInternAtom(tdpy, "_MOTIF_WM_HINTS", True);
    XChangeProperty(tdpy, twin, property, property, 32, PropModeReplace, (unsigned char *) &hints, 5);
    // end Hide window border

    int xmouse, ymouse;
    while(1)
    {
        usleep(15000);
        if(!use_shm)
        {
            image = capture_root(sdpy, sscr);
            draw_image(tdpy, twin, image);
            XDestroyImage(image);
        }
        else
        {
            XShmGetImage(sdpy, swin, simage, 0, 0, AllPlanes);
            if(!emulate_events)
            {
                Window rwin, cwin;
                int xmouse, ymouse, x, y;
                unsigned int mask;
                XQueryPointer(sdpy, swin, &rwin, &cwin, &xmouse, &ymouse, &x, &y, &mask);
                XFixesCursorImage* cur = XFixesGetCursorImage(sdpy);
                drawMouse(timage, cur, xmouse, ymouse);
            }

            XShmPutImage(tdpy, twin, tgc, timage, 0, 0, 0, 0, timage->width, timage->height, False);
        }
        XFlush(tdpy);
    }
    return 0;
}
