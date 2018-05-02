/*
 * Custom bar components.
 */

#ifndef CUSTOM_H_
#define CUSTOM_H_

#include "bar.h"
using namespace gfx;
using namespace bar;

#include <time.h>
#include <fstream>
#include <string>
#include <memory>
#include <stdio.h>
#include <array>
#include <alsa/asoundlib.h>
#include <algorithm>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

namespace custom {
    class Back : public Component {
        public:
        virtual void update(Event const& ev) {
            fill_back(0, WIDTH, "black");
        }
        virtual ETList get_relevant_event_types() const {
            return {Startup};
        }
    };

    template<Coord startx, Coord width>
    class Clock : public Component {
    private:
        Text text;

    public:
        Clock() : text("main", "white") {}
        virtual void update(Event const& ev) {
            // get the time
            time_t now_raw = time(NULL);
            struct tm *now = localtime(&now_raw);
            char buff[10];
            strftime(buff, 10, "%I:%M:%S", now);

            // remove leading 0
            std::string timestr = buff;
            if (timestr[0] == '0') {
                timestr = timestr.substr(1, timestr.length()-1);
            }

            // print
            fill_back(startx, width, "black");
            text = timestr;
            text.draw(startx+(width/2));
        }
        virtual ETList get_relevant_event_types() const {
            return {Update};
        }
    };

    template<Coord startx, Coord width>
    class Brightness : public Component {
    private:
        Text text;
        int32_t max_brightness;
        int32_t last;
        bool sym_mode; // true ==> symbol mode; false ==> text mode

    public:
        Brightness() : text("main", "white"), sym_mode(true) {
            std::ifstream fin(
                    "/sys/class/backlight/intel_backlight/max_brightness");
            fin >> max_brightness;
            fin.close();

            sym_mode = true;
        }
        virtual void update(Event const& ev) {
            if (ev.type == ButtonPress && ev.xbutton.x >= startx
                    && ev.xbutton.x < startx+width) {
                sym_mode = !sym_mode;
            }

            // get current brightness
            std::ifstream fin(
                    "/sys/class/backlight/intel_backlight/brightness");
            int32_t brightness;
            fin >> brightness;
            fin.close();
            int32_t percent = (100*brightness)/max_brightness;

            // translate into text
            text.col = percent >= 66 ? "red" : "white";
            if (sym_mode && (percent == last)) {
                text.fnt = "symbol";
                text = percent < 33 ? u8"\uf006"  // empty star
                     : percent < 66 ? u8"\uf123"  // half-full star
                     :                u8"\uf005"; // full star
            }
            else {
                text.fnt = "main";
                text = std::to_string(percent) + "%";
            }
            last = percent;

            // draw
            fill_back(startx, width, "black");
            text.draw(startx+(width/2));
        }
        virtual ETList get_relevant_event_types() const {
            return {Update, ButtonPress};
        }
    };

    template<Coord startx, Coord width>
    class Battery : public Component {
        Text text;
        bool sym_mode;

    public:
        Battery() : text("main", "white"), sym_mode(true) {}
        virtual void update(Event const& ev) {
            if (ev.type == ButtonPress && ev.xbutton.x >= startx
                    && ev.xbutton.x < startx+width) {
                sym_mode = !sym_mode;
            }
            text.fnt = sym_mode ? "symbol" : "main";
            
            // get charging status
            std::string status;
            std::ifstream fin("/sys/class/power_supply/BAT1/status");
            fin >> status;
            fin.close();
            bool charging = (status != "Discharging");

            // get charge
            int charge;
            fin.open("/sys/class/power_supply/BAT1/capacity");
            fin >> charge;
            fin.close();

            // draw
            text.col = (charge < 25) ? "red"
                     : charging      ? "green"
                     :                 "white";
            if (sym_mode) {
                text = charge < 25 ? (
                        charging ? u8"\uf244"    // empty battery
                                 : std::to_string(charge) + "%")
                     : charge < 50 ? u8"\uf243"  // 25% battery
                     : charge < 75 ? u8"\uf242"  // 50% battery
                     : charge < 90 ? u8"\uf241"  // 75% battery
                     :               u8"\uf240"; // full battery
            }
            else {
                text = std::to_string(charge) + "%";
            }
            fill_back(startx, width, "black");
            text.draw(startx+(width/2));
        }
        virtual ETList get_relevant_event_types() const {
            return {Update, ButtonPress};
        }
    };

    template<Coord startx, Coord width>
    class Wifi : public Component {
        Text text;

    public:
        Wifi() : text("symbol", "white") {}
        virtual void update(Event const& ev) {
            // ping google DNS
            std::unique_ptr<FILE, decltype(&pclose)> ping(
                    popen("ping -c 1 -s 0 8.8.8.8 -w 1 2>&1", "r"),
                    pclose);
            char first = fgetc(ping.get());
            bool connected = (first == 'P');

            // draw
            text = connected ? u8"\uf1eb"  // wifi
                             : u8"\uf127"; // broken chain
            text.col = connected ? "white" : "red";
            fill_back(startx, width, "black");
            text.draw(startx+(width/2));
        }
        virtual ETList get_relevant_event_types() const {
            return {Update};
        }
    };

    template<Coord startx, Coord width>
    class Volume : public Component {
        Text text;
        bool sym_mode;
        long last;

    public:
        Volume() : text("symbol", "white"), sym_mode(true) {}
        virtual void update(Event const& ev) {
            if (ev.type == ButtonPress && ev.xbutton.x >= startx
                    && ev.xbutton.x < startx+width) {
                sym_mode = !sym_mode;
            }

            // get volume
            snd_mixer_t *handle;
            snd_mixer_open(&handle, 0);
            snd_mixer_attach(handle, "default");
            snd_mixer_selem_register(handle, NULL, NULL);
            snd_mixer_load(handle);
            snd_mixer_selem_id_t *sid;
            snd_mixer_selem_id_alloca(&sid);
            snd_mixer_selem_id_set_index(sid, 0);
            snd_mixer_selem_id_set_name(sid, "Master");
            snd_mixer_elem_t *elem = snd_mixer_find_selem(handle, sid);
            long vol_min, vol_max;
            snd_mixer_selem_get_playback_volume_range(
                    elem, &vol_min, &vol_max);
            long volume;
            snd_mixer_selem_get_playback_volume(
                    elem, SND_MIXER_SCHN_MONO, &volume);
            int not_muted;
            snd_mixer_selem_get_playback_switch(
                    elem, SND_MIXER_SCHN_MONO, &not_muted);
            snd_mixer_close(handle);
            long percent = (100*(volume-vol_min))/vol_max;

            text.col = (!not_muted) ? "red" : "white";

            // draw
            if (sym_mode && (percent == last)) {
                text.fnt = "symbol";
                text = percent < 50 ? u8"\uf027"  // speaker w/ no waves
                                    : u8"\uf028"; // speaker w/ waves
            }
            else {
                text.fnt = "main";
                text = std::to_string(percent) + "%";
            }
            last = percent;
            fill_back(startx, width, "black");
            text.draw(startx+(width/2));
        }
        virtual ETList get_relevant_event_types() const {
            return {Update, ButtonPress};
        }
    };

    template<Coord startx, Coord width>
    class Taskbar : public Component {
        int const TGT_WIDTH = 100; // the width of each icon-region

        Text text;
        std::vector<std::pair<Window, std::string>> wnd_list;
        Window active;
        int active_wnd_idx;

        Atom NET_CLIENT_LIST;
        Atom NET_ACTIVE_WINDOW;

        KeyCode alt_kc, tab_kc, grave_kc;
        unsigned int alt_mask;
        bool alttab_mode;
        int atsel_wnd_idx;

        void refresh_list() {
            // get list of wm-managed windows.
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            long *prop;
            XGetWindowProperty(
                    dpy, root,
                    NET_CLIENT_LIST,
                    0, (~0L), false, AnyPropertyType, &actual_type,
                    &actual_format, &nitems, &bytes_after,
                    (unsigned char**)&prop);

            // assign icons to each
            wnd_list.clear();
            XClassHint class_hint;
            for (int i = 0; i < nitems; i++) {
                Window w = prop[i];
                if (XGetClassHint(dpy, w, &class_hint)) {
                    // get WM class (usually something name-like)
                    std::string wm_class = class_hint.res_class;
                    XFree(class_hint.res_name);
                    XFree(class_hint.res_class);
                    if (wm_class.empty()) {
                        continue;
                    }

                    // assign icons, add to list
                    std::string icon;
                    icon = wm_class == "URxvt"   ? u8"\uf120"  // terminal
                         : wm_class == "Firefox" ? u8"\uf269"  // firefox logo
                         :                         u8"\uf059"; // ? mark
                    wnd_list.emplace_back(w, icon);
                }
            }
            XFree(prop);
            // sort into a persistent ordering (Window ~ int)
            std::sort(wnd_list.begin(), wnd_list.end(),
                    [](auto const& a, auto const& b){return a.first < b.first;});
        }

        void refresh_active() {
            Atom actual_type;
            int actual_format;
            unsigned long nitems, bytes_after;
            long *prop;
            XGetWindowProperty(
                    dpy, root,
                    NET_ACTIVE_WINDOW,
                    0, (~0L), false, AnyPropertyType, &actual_type,
                    &actual_format, &nitems, &bytes_after,
                    (unsigned char**)&prop);
            active_wnd_idx = -1;
            if (nitems > 0) {
                active = prop[0];
                for (int i = 0; i < wnd_list.size(); i++) {
                    if (wnd_list[i].first == active) {
                        active_wnd_idx = i;
                        break;
                    }
                }
            }
            XFree(prop);
        }

        void activate_window(Window tgt) {
            XClientMessageEvent msg;
            msg.type = ClientMessage;
            msg.message_type = NET_ACTIVE_WINDOW;
            msg.window = tgt;
            msg.format = 32;
            msg.data.l[0] = 1;
            msg.data.l[1] = msg.data.l[2] = msg.data.l[3] = msg.data.l[4]
                = 0;
            XSendEvent(dpy, root, false,
                    SubstructureNotifyMask | SubstructureRedirectMask,
                    (XEvent*)(&msg));
            XFlush(dpy);
        }

        void click(int x) {
            int target = (x - int(startx))/TGT_WIDTH;
            Window w;
            if (target >= 0 && target < wnd_list.size()) {
                w = wnd_list[target].first;
            }
            else {
                return;
            }
            activate_window(w);
        }

        void set_keyreleasemask(Window win) {
            // select input on given; don't use XSelectInput because that just
            // replaces the old input mask (so any previous mask is just
            // tossed away)
            XWindowAttributes get_attrs;
            XGetWindowAttributes(gfx::dpy, win, &get_attrs);
            XSetWindowAttributes set_attrs;
            set_attrs.event_mask =
                get_attrs.your_event_mask|KeyReleaseMask|SubstructureNotifyMask;
            XChangeWindowAttributes(gfx::dpy, win, CWEventMask, &set_attrs);

            // select input on all children of given
            Window root, parent;
            Window *children;
            unsigned int nchildren;
            if (!XQueryTree(
                        gfx::dpy, win, &root, &parent, &children, &nchildren)) {
                return;
            }

            while (nchildren--) {
                set_keyreleasemask(children[nchildren]);
            }
            if (children) {
                XFree(children);
            }
        }

    public:
        Taskbar() : text("symbol", "white") {
            NET_CLIENT_LIST = XInternAtom(dpy, "_NET_CLIENT_LIST", true);
            NET_ACTIVE_WINDOW = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", true);

            tab_kc = XKeysymToKeycode(gfx::dpy, XK_Tab);
            alt_kc = XKeysymToKeycode(gfx::dpy, XK_Alt_L);
            grave_kc = XKeysymToKeycode(gfx::dpy, XK_grave);
            alt_mask = Mod1Mask;
            set_keyreleasemask(gfx::root);
            XGrabKey(gfx::dpy, tab_kc, alt_mask, gfx::root, true,
                    GrabModeAsync, GrabModeAsync);
            XGrabKey(gfx::dpy, grave_kc, alt_mask, gfx::root, true,
                    GrabModeAsync, GrabModeAsync);
            alttab_mode = false;
        }
        virtual void update(Event const& ev) {
            if (ev.type == Startup) {
                refresh_list();
                refresh_active();
            }
            else if (ev.type == PropertyNotify) {
                if (ev.xproperty.atom == NET_CLIENT_LIST) {
                    refresh_list();
                }
                else if (ev.xproperty.atom == NET_ACTIVE_WINDOW) {
                    refresh_active();
                }
            }
            else if (ev.type == ButtonPress && ev.xbutton.x >= startx
                    && ev.xbutton.x < startx+width) {
                click(ev.xbutton.x);
            }
            else if (ev.type == MapNotify) {
                set_keyreleasemask(ev.xmap.window);
            }
            else if (ev.type == KeyPress && ev.xkey.state == alt_mask
                    && !wnd_list.empty()
                    && (ev.xkey.keycode == tab_kc
                        || ev.xkey.keycode == grave_kc)) {
                // either alt-tab was pressed for the first time, or the user
                // pressed alt-tab earlier and is now cycling by holding alt
                // and repeatedly pressing tab.
                if (!alttab_mode) {
                    alttab_mode = true;
                    atsel_wnd_idx = active_wnd_idx;
                }
                if (ev.xkey.keycode == tab_kc) {
                    // tab --> forward
                    atsel_wnd_idx = (atsel_wnd_idx+1) % wnd_list.size();
                }
                else {
                    // grave --> back
                    atsel_wnd_idx = (atsel_wnd_idx > 0) ? atsel_wnd_idx-1
                                                        : wnd_list.size()-1;
                }
            }
            else if (ev.type == KeyRelease && ev.xkey.keycode == alt_kc
                    && alttab_mode) {
                // the user released the alt key; end the window selection
                alttab_mode = false;
                if (atsel_wnd_idx >= 0 && atsel_wnd_idx < wnd_list.size()) {
                    activate_window(wnd_list[atsel_wnd_idx].first);
                }
            }
            else {
                return; // ==> no redraw
            }

            // redraw
            fill_back(startx, width, "black");
            for (int i = 0; i < wnd_list.size(); i++) {
                int x = startx+(TGT_WIDTH*i);

                // if the alttab-sel'd window is the same as the currently
                // active window, draw as an alttab-sel'd window.
                if (alttab_mode && i == atsel_wnd_idx) {
                    // use white on red to highlight
                    fill_back(x, TGT_WIDTH, "red");
                }
                else if (!alttab_mode && wnd_list[i].first == active) {
                    // use black on white to highlight
                    text.col = "black";
                    fill_back(x, TGT_WIDTH, "white");
                }

                text = wnd_list[i].second;
                text.draw(x+(TGT_WIDTH/2));

                // reset the color to normal (if it changed...)
                text.col = "white";
            }
        }
        virtual ETList get_relevant_event_types() const {
            return {Startup, ButtonPress, PropertyNotify, KeyPress,
                KeyRelease, MapNotify};
        }
    };

    void init() {
        add_color("black", 0x2a2a2a);
        add_color("white", 0xeeeeee);
        add_color("red",   0xbd5a4e);
        add_color("green", 0xb5bd68);

        add_font("main", "noto:size=22");
        add_font("symbol", "fontawesome:size=22");

        comps.emplace_back(new Back());
        comps.emplace_back(new Taskbar<100, 1300>());
        comps.emplace_back(new Clock<1400, 400>());
        comps.emplace_back(new Wifi<2700, 100>());
        comps.emplace_back(new Volume<2800, 100>());
        comps.emplace_back(new Brightness<2900, 100>());
        comps.emplace_back(new Battery<3000, 100>());
    }
}

#endif // CUSTOM_H_
