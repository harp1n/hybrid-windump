// gcc -O3 -o windump windump.c -lX11 -lXfixes -lXext -lXtst
// gcc -O3 -o windump windump.c -lX11 -lXfixes -lXext

#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xos.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XShm.h>
#ifdef USE_XTEST
#include <X11/extensions/XTest.h>
#endif
#include <X11/extensions/Xfixes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

int format = ZPixmap;
typedef struct
{
    unsigned   flags;
    unsigned   functions;
    unsigned   decorations;
    int            inputMode;
    unsigned   status;
} Hints;



double time_s()
// gives back time in microseconds
{
#ifndef _WIN32
    struct timeval tv;
    struct timezone tz;
    tz.tz_minuteswest = 0;
    tz.tz_dsttime     = 0;
    gettimeofday(&tv,&tz);
    return ( (double)tv.tv_sec+(double)tv.tv_usec*1E-6 );
#else
	union{
		long int ns100;
		FILETIME ft;
	} now;
	GetSystemTimeAsFileTime(&now.ft);
    return( ((double)now.ns100)*1.0E-7 );
#endif
}


XImage *
CaptRoot(Display * dpy, int screen)
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
    int                 transparentOverlays , multiVis;
    int                 numVisuals;
    XVisualInfo         *pVisuals;
    int                 numOverlayVisuals;
    OverlayInfo         *pOverlayVisuals;
    int                 numImageVisuals;
    XVisualInfo         **pImageVisuals;
    list_ptr            vis_regions;    /* list of regions to read from */
    list_ptr            vis_image_regions ;
    Visual		vis_h,*vis ;
#endif
    int			allImage = 0 ;

    Window window=RootWindow (dpy, screen);

    if (!XGetWindowAttributes(dpy, window, &win_info))
    { fprintf(stderr,"Can't get target window attributes."); exit(1); }

    absx=0; absy=0;
    win_info.x = 0;
    win_info.y = 0;
    width = win_info.width;
    height = win_info.height;
    bw = 0;

    dwidth = DisplayWidth (dpy, screen);
    dheight = DisplayHeight (dpy, screen);

    XFetchName(dpy, window, &win_name);
    if (!win_name || !win_name[0]) {
	win_name = "xwdump";
	got_win_name = False;
    } else {
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
	image = XGetImage (dpy, window, x, y, width, height,
			   AllPlanes, format);
    if (!image) {
	fprintf (stderr, "unable to get image at %dx%d+%d+%d\n",
		 width, height, x, y);
	exit (1);
    }

    //if (add_pixel_value != 0) XAddPixel (image, add_pixel_value);

    return image;
}

Window CreateWindow(Display * dpy, int screen, int width, int height, int windowdec)
{
    XSetWindowAttributes winAttr;
    winAttr.override_redirect=((windowdec==0)?True:False);

    //printf("w=%d h=%d dpy=%p scr=%d\n",width,height,dpy,screen);
    Window win = 0;
    win = XCreateWindow(dpy, RootWindow(dpy, screen),
                        0, 0, width, height,
                        0,
                        24,
                        InputOutput,
                        DefaultVisual(dpy, screen),
                        //CWBorderPixel|CWBackPixel|CWColormap|CWEventMask|CWBitGravity,
                        CWOverrideRedirect, &winAttr);

    XSizeHints sizeHints;
    sizeHints.flags = PPosition | PSize;
    sizeHints.x = 0;
    sizeHints.y = 0;
    sizeHints.width = width;
    sizeHints.height = height;

    XSetNormalHints(dpy,win,&sizeHints);

    XSelectInput(dpy, win, StructureNotifyMask|ButtonPressMask|ButtonReleaseMask);

    XMapWindow(dpy, win);
    return win;
}

void DrawImage(Display * dpy, Window win, XImage * image)
{
    static GC gc;
    if(win!=0)
    {
        if(gc==0)
        {
            XGCValues gc_val;
            gc = XCreateGC (dpy, win, GCForeground|GCBackground, &gc_val);
        }
        XPutImage (dpy, win, gc, image, 0, 0, 0, 0, image->width, image->height);
    }
}

void createShmImage(int w, int h, XShmSegmentInfo * sinfo, XShmSegmentInfo * tinfo, Display * sdpy, Display * tdpy, int sscr, int tscr, XImage ** simage, XImage ** timage)
{
    sinfo->shmid=tinfo->shmid=shmget(IPC_PRIVATE,w*h*sizeof(unsigned),IPC_CREAT|0666 );
    sinfo->shmaddr=tinfo->shmaddr=(char*)shmat(sinfo->shmid,0,0);
    sinfo->readOnly=True;
    tinfo->readOnly=False;
    //printf("%d %d\n",DefaultDepth(sdpy,sscr),DefaultDepth(tdpy,tscr));
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

void drawMouse(XImage * img, int xm, int ym, int color)
{
    int x,y,x1,y1,x2,y2,w,h;

    unsigned * data = (unsigned *)(img->data);

    w=img->width;
    h=img->height;

    x=xm; if(x<0) x=0; if(x>=w) x=w-1;
    y=ym; if(y<0) y=0; if(y>=h) y=h-1;
    x1=xm-5; if(x1<0) x1=0; if(x1>w) x1=w;
    x2=xm+6; if(x2<0) x2=0; if(x2>w) x2=w;
    for(x=x1;x<x2;x++) data[y*w+x] = color;
    //data[y*w+x] = 0xFFFFFF;

    x=xm; if(x<0) x=0; if(x>=w) x=w-1;
    y=ym; if(y<0) y=0; if(y>=h) y=h-1;
    y1=ym-5; if(y1<0) y1=0; if(y1>h) y1=h;
    y2=ym+6; if(y2<0) y2=0; if(y2>h) y2=h;
    for(y=y1;y<y2;y++) data[y*w+x] = color;
}

// Clamp a value between a lower and upper bound, inclusive.
#define CLAMP(x, l, u) ((x) < (l) ? (l) : ((x) > (u) ? (u) : (x)))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

void drawMouseImage(XImage* img, XFixesCursorImage* cur, int xm, int ym)
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

modifyMouseCursor(XFixesCursorImage* cur)
{
    int x,y;
    for( y=0 ; y<cur->height ; y++ )
    {
        for( x=0 ; x<cur->width ; x++ )
        {
            cur->pixels[y * cur->width + x]&=0xAFFF0000;
        }
    }
}


void emulateEvents(Display * sdpy, Window swin, Display * tdpy, Window twin)
{
#ifdef USE_XTEST
    int xmouse, ymouse;
            XEvent e;

            //long mask=ButtonPressMask|ButtonReleaseMask|MotionNotifyMask;
            //while(XCheckWindowEvent(tdpy, twin, mask, &e)!=False)
            //while( XCheckTypedWindowEvent(tdpy, twin, ButtonPress,   &e)!=False ) XPutBackEvent( sdpy, &e );
            //while( XCheckTypedWindowEvent(tdpy, twin, ButtonRelease, &e)!=False ) XPutBackEvent( sdpy, &e );

            while(XCheckTypedWindowEvent(tdpy, twin, ButtonPress,   &e)!=False ||
                  XCheckTypedWindowEvent(tdpy, twin, ButtonRelease, &e)!=False )
            {
                printf("button event\n");
                //if(emulate_events)
                {
                    /*e.xbutton.display=sdpy;
                    e.xbutton.window=swin;
                    e.xbutton.root=swin;
                    e.xbutton.window=swin;
                    e.xbutton.x_root=e.xbutton.x;
                    e.xbutton.y_root=e.xbutton.y;*/
                    //XSendEvent( sdpy, swin, True, mask, &e );
                    XPutBackEvent( sdpy, &e );
                    //XTestFakeMotionEvent(sdpy,sscr,e.xbutton.x,e.xbutton.y,0);
                    XTestFakeButtonEvent(sdpy,e.xbutton.button,e.xbutton.type==ButtonPress,0);
                }
            }
            while(XCheckTypedWindowEvent(sdpy, swin, MotionNotify, &e)!=False)
            {
                //XPutBackEvent( sdpy, &e );
                //printf("motion event\n");
                xmouse=e.xbutton.x_root;
                ymouse=e.xbutton.y_root;
            }
#endif
}

void printUsage()
{
        printf("\nusage: windump [srcdpy [destdpy]] [-s srcdpy] [-t destdpy] [-d delay_us] [-w windowdec] [-a autohide] [-i srcwinID] [srcdpy [destdpy]]\n");
        printf("\n");
        printf("       srcdpy ...... source display to capture\n");
        printf("                       [default: :0.0]\n");
        printf("       destdpy ..... target display where to display the captured screen\n");
        printf("                       [default: :0.1]\n");
        printf("       delay_us .... time delay between frame captures in mycroseconds\n");
        printf("                       [default: 15000]\n");
        printf("       windowdec ... enable window decoration\n");
        printf("                       [default: 0]\n");
        printf("       autohide .... auto hide captured screen\n");
        printf("                       [default: 1]\n");
        printf("       srcwinID .... window ID (in hex) to be captured if not root window\n");
        printf("                       [default: root window]\n");
        printf("\n");
}

main(int argc, char * argv [])
{
    int opt;
    if (argc<=1)
    {
        //printHelp();
    }
    char * sdpyName = ":0.0";
    char * tdpyName = ":0.1";

    Window swin=0;

    int delay_us=15000;
    int windowdec=0;
    int autohide=1;
#define MYBOOL(x) ((int)((x)[0]=='y'||(x)[0]=='1'||(x)[0]=='t'))
    if ( argc>1 && argv[1][0]!='-' ) {
        sdpyName=argv[1]; argv[1]=argv[0]; argv=&(argv[1]); argc--;
        if ( argc>1 && argv[1][0]!='-' ) {
            tdpyName=argv[1]; argv[1]=argv[0]; argv=&(argv[1]); argc--;
        }
    }
    while ((opt = getopt(argc, argv, "s:t:d:w:a:i:")) != -1) {
        switch (opt) {
            case 's': sdpyName=strdup(optarg); break;
            case 't': tdpyName=strdup(optarg); break;
            case 'd': delay_us=atoi(optarg); break;
            case 'w': windowdec=MYBOOL(optarg); break;
            case 'a': autohide=MYBOOL(optarg); break;
            case 'i': sscanf(optarg,"%x",(unsigned int *)&swin); break;
            default: printUsage(); exit(1); break;
        }
    }

    if (optind  <argc) sdpyName=argv[optind];
    if (optind+1<argc) tdpyName=argv[optind+1];
    //if (argc>2) tdpyName=argv[2];
    Display * sdpy = XOpenDisplay(sdpyName);
    Display * tdpy = XOpenDisplay(tdpyName);
    int sscr = XDefaultScreen(sdpy);
    int tscr = XDefaultScreen(tdpy);
    GC tgc = DefaultGC(tdpy,tscr);
    if(swin==0) swin=RootWindow (sdpy,sscr);
    //if (argc>3) swin=atoi(argv[3]);
    int width, height, dummy;
    XGetGeometry(sdpy, swin, (Window *)&dummy, &dummy, &dummy, &width, &height, &dummy, &dummy);
    Window twin=CreateWindow(tdpy,tscr,width,height,windowdec);
    XSelectInput(sdpy, swin, PointerMotionMask);
    XImage * image;
    XImage * simage;
    XImage * timage;
    int use_shm=1;
    int use_xv=0;
    XShmSegmentInfo xshm_sinfo;
    XShmSegmentInfo xshm_tinfo;
    if(use_shm) createShmImage(width,height,&xshm_sinfo,&xshm_tinfo,sdpy,tdpy,sscr,tscr,&simage,&timage);

    int frame=0;
    for(;;) {
        XEvent e;
        XNextEvent(tdpy, &e);
        if (e.type == MapNotify)
            break;
    }

    int emulate_events=0;

#if 1
    Hints   hints;
    Atom    property;
    hints.flags = 2;        // Specify that we're changing the window decorations.
    hints.functions=0;
    hints.decorations = 0;  // 0 (false) means that window decorations should go bye-bye.
    property = XInternAtom(tdpy, "_MOTIF_WM_HINTS", True);
    XChangeProperty(tdpy, twin, property, property, 32, PropModeReplace, (unsigned char *) &hints, 5);


    XSetWindowAttributes attributes;

    //attributes.override_redirect = True;
    //XChangeWindowAttributes(tdpy, twin,
    //                        CWOverrideRedirect, &attributes);
#endif

    double t;
    while(1)
    {

        if(emulate_events) emulateEvents(sdpy,swin,tdpy,twin);

        //{ static double t=0.0; if (t==0.0) t=time_s(); printf("fps %f\n", 1.0/(time_s()-t)); t=time_s(); }

        //t=time_s();
        usleep(delay_us);
        //printf("sleep: %f sec\n", time_s()-t);

        Window rwin,cwin;
        int xmouse,ymouse,x,y,mask;
        XQueryPointer(sdpy,swin,&rwin,&cwin,&xmouse,&ymouse,&x,&y,&mask);
        if( (x==xmouse && y==ymouse) || !autohide )
        {
            if(autohide)
            {
                XResizeWindow(tdpy,twin,width,height);
                XRaiseWindow(tdpy,twin);
            }

            if(use_xv)
            {
                //XvShmGetImage (sdpy, swin, simage, 0, 0, AllPlanes);
            }
            else if(use_shm)
            {
                int use_damage=0;
                if(use_damage)
                {
                }
                else {
                    //t=time_s();
                    XShmGetImage (sdpy, swin, simage, 0, 0, AllPlanes);
                    //printf("get: %f sec\n", time_s()-t);

                    //drawMouse(timage, xmouse+1, ymouse+1,0x000000);
                    //drawMouse(timage, xmouse, ymouse,0xFFFFFF);
                    XFixesCursorImage* cur = XFixesGetCursorImage(sdpy);
                    //modifyMouseCursor(cur);
                    drawMouseImage(timage, cur, xmouse, ymouse);
                    XFree(cur);

                    //t=time_s();
                    XShmPutImage (tdpy, twin, tgc, timage, 0, 0, 0, 0, timage->width, timage->height, False);
                    //printf("put: %f sec\n", time_s()-t);
                }
            }
            else
            {
                //t=time_s();
                image=CaptRoot(sdpy,sscr);
                //printf("get: %f sec\n", time_s()-t);

                //t=time_s();
                DrawImage(tdpy,twin,image);
                XDestroyImage(image);
                //printf("put: %f sec\n", time_s()-t);
            }
        }
        else
        {
            if(autohide)
            {
                XResizeWindow(tdpy,twin,1,1);
            }
        }
        //t=time_s();
        //XFlush(tdpy);
        XSync(tdpy,False);
        //printf("flush: %f sec\n", time_s()-t);
    }
}
