/*
 * Main source for the "cybar".
 * Author: Matthew Bauer
 */

#include "bar.h"

#include <iostream>
#include <codecvt>
#include <locale>
#include <thread>

#include <X11/Xatom.h>

#include "custom.h"

int silent_xerror_handler(Display*, XErrorEvent*) {
    return 0;
}

/* gfx:: implementations. */
std::unordered_map<std::string, gfx::XftColorWrapper> gfx::colors;
gfx::XftColorWrapper::XftColorWrapper() : XftColorWrapper(0) {}
gfx::XftColorWrapper::XftColorWrapper(uint32_t val) {
    // XRenderColors have 16 bits per field! (not 8)
    // ==> must convert e.g. 0xff to 0xffff
    XRenderColor xrcol;
    xrcol.red   = ((val >> 16) & 0xff) * (0xffff/0xff);
    xrcol.green = ((val >> 8)  & 0xff) * (0xffff/0xff);
    xrcol.blue  = (val         & 0xff) * (0xffff/0xff);
    xrcol.alpha = 0xffff;

    if (!XftColorAllocValue(dpy, vis, cmap, &xrcol, &col)) {
        throw bar::Error("failed to allocate color with value: ", std::hex,
                val);
    }
}
void gfx::add_color(std::string const& name, uint32_t val) {
    if (colors.count(name)) {
        throw bar::Error("color name used twice: ", name);
    }
    try {
        colors.emplace(name, XftColorWrapper(val));
    }
    catch (bar::Error const& err) {
        throw err + bar::Error("in adding of color with name: ", name);
    }
}
gfx::XftColorWrapper *gfx::get_color(std::string const& name) {
    if (!colors.count(name)) {
        throw bar::Error("color not found with name: ", name);
    }
    return &(colors[name]);
}
gfx::Color::Color(std::string const& name) : wrap(get_color(name)) {}
gfx::Color::Color(char const *name) : Color(std::string(name)) {}

std::unordered_map<std::string, gfx::XftFontWrapper> gfx::fonts;
gfx::XftFontWrapper::XftFontWrapper() : fnt(nullptr) {}
gfx::XftFontWrapper::XftFontWrapper(std::string const& spec) {
    fnt = XftFontOpenName(dpy, DefaultScreen(dpy), spec.c_str());
    if (!fnt) {
        throw bar::Error("failed to load font with spec: ", spec);
    }
}
gfx::XftFontWrapper::XftFontWrapper(XftFontWrapper&& f) : fnt(nullptr) {
    std::swap(fnt, f.fnt);
}
gfx::XftFontWrapper::~XftFontWrapper() {
    if (fnt) {
        XftFontClose(dpy, fnt);
    }
}
void gfx::add_font(std::string const& name, std::string const& spec) {
    if (fonts.count(name)) {
        throw bar::Error("font name used twice: ", name);
    }
    try {
        fonts.emplace(name, std::move(XftFontWrapper(spec)));
    }
    catch (bar::Error const& err) {
        throw err + bar::Error("in adding of font with name: ", name);
    }
}
gfx::XftFontWrapper *gfx::get_font(std::string const& name) {
    if (!fonts.count(name)) {
        throw bar::Error("font not found with name: ", name);
    }
    return &(fonts[name]);
}
gfx::Font::Font(std::string const& name) : wrap(get_font(name)) {}
gfx::Font::Font(char const *name) : Font(std::string(name)) {}

gfx::Text::Text(gfx::Font f, gfx::Color c, std::string const& u8s)
    : fnt(f), col(c) {
    *this = u8s;
}
gfx::Text& gfx::Text::operator=(std::string const& u8str) {
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    u16 = convert.from_bytes(u8str);
    return *this;
}
void gfx::Text::draw(gfx::Coord x) const {
    XGlyphInfo extents;
    XftFont *f = fnt.wrap->fnt;
    XftTextExtents16(dpy, f, (FcChar16*)u16.data(), u16.size(), &extents);
    int text_w = extents.width;
    int text_h = f->ascent - f->descent;
    int text_x = x - (text_w/2);
    int text_y = (HEIGHT + text_h)/2;
    XftDrawString16(xft_draw, &(col.wrap->col), f, text_x, text_y,
            (FcChar16*)u16.data(), u16.size());
}

void gfx::fill_back(gfx::Coord x, gfx::Coord w, gfx::Color col) {
    XftDrawRect(xft_draw, &(col.wrap->col), x, 0, w, HEIGHT);
}

void gfx::flip() {
    XSetWindowBackgroundPixmap(dpy, wnd, backbuffer);
    XClearWindow(dpy, wnd);
    XFlush(dpy);
}

void gfx::init() {
    XInitThreads();
    XSetErrorHandler(&silent_xerror_handler);

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        throw bar::Error("failed to open X connection");
    }

    // save various info
    screen = DefaultScreen(dpy);
    cmap = DefaultColormap(dpy, screen);
    vis = DefaultVisual(dpy, screen);
    root = DefaultRootWindow(dpy);

    // create window
    XSetWindowAttributes wnd_attrs;
    wnd_attrs.override_redirect = true;
    wnd = XCreateWindow(dpy, DefaultRootWindow(dpy),
            0, 0, WIDTH, HEIGHT,
            0, CopyFromParent, InputOutput, vis, CWOverrideRedirect, &wnd_attrs);
    backbuffer = XCreatePixmap(dpy, wnd, WIDTH, HEIGHT, 24);
    xft_draw = XftDrawCreate(dpy, backbuffer, vis, cmap);
    if (!xft_draw) {
        throw bar::Error("failed to create XftDraw");
    }

    // tell the WM that this is a bar and shouldn't be messed with
    Atom atom_wmtype_dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", false);
    XChangeProperty(dpy, wnd,
            XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", false), XA_ATOM, 32,
            PropModeAppend, (unsigned char*)(&atom_wmtype_dock), 1);

    XSelectInput(dpy, wnd, 0
            | ExposureMask | ButtonReleaseMask | ButtonPressMask
            | KeyPressMask | KeyReleaseMask);
    XSelectInput(dpy, root, 0
            | SubstructureNotifyMask | PropertyChangeMask);
    XMapRaised(dpy, wnd);
}

Display *gfx::dpy;
int gfx::screen;
Pixmap gfx::backbuffer;
Colormap gfx::cmap;
Visual *gfx::vis;
Window gfx::wnd, gfx::root;
XftDraw *gfx::xft_draw;

/* bar:: implementations. */
bar::Component::~Component() {}
std::vector<std::unique_ptr<bar::Component>> bar::comps;

void bar::run() {
    // link event type --> relevant components
    std::unordered_map<EventType, std::vector<Component*>> et_table;
    for (auto const& c : comps) {
        for (EventType t : c->get_relevant_event_types()) {
            if (!et_table.count(t)) {
                et_table.emplace(t, std::vector<Component*>());
            }
            et_table[t].push_back(c.get());
        }
    }

    // start the exposure thread
    bool stop = false;
    std::thread heartbeat_thread([stop](){
            XExposeEvent ev;
            ev.type = Expose;
            ev.display = gfx::dpy;
            ev.window = gfx::wnd;
            ev.x = ev.y = 0;
            ev.width = gfx::WIDTH;
            ev.height = gfx::HEIGHT;
            ev.count = 0;
            ev.send_event = true;
            while (!stop) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                XSendEvent(gfx::dpy, gfx::wnd, false, ExposureMask, (XEvent*)(&ev));
                XFlush(dpy);
            }
        });

    XEvent ev;
    auto dispatch = [&ev, &et_table] () mutable {
        if (et_table.count(ev.type)) {
            for (Component *c : et_table[ev.type]) {
                c->update(ev);
            }
            gfx::flip();
        }
    };
    // send out Startup event
    ev.type = Startup;
    dispatch();
    // event loop
    while (true) {
        XNextEvent(gfx::dpy, &ev);
        if (ev.type == FocusIn) {
            std::cerr << "b\n";
        }
        dispatch();
    }
}

int main() {
    try {
        gfx::init();
        custom::init();
        bar::run();
    }
    catch (bar::Error const& err) {
        std::cerr << "E: " << err << std::endl;
    }
}
