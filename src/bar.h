/*
 * Main cyanbar extension interface.
 * Author: Matthew Bauer
 */

#ifndef BAR_H_
#define BAR_H_

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <stdint.h>
#include <functional>
#include <sstream>
#include <memory>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

/** Interface containing graphics-related utilities. */
namespace gfx {
    /** Coordinate abstraction. */
    using Coord = uint32_t;

    /** Bar dimensions. */
    Coord const WIDTH = 3200;
    Coord const HEIGHT = 50;

    /** Wrapper around an XftColor. */
    struct XftColorWrapper {
        /** Black. */
        XftColorWrapper();
        /** Initialize with 0xRRGGBB formatted value. */
        XftColorWrapper(uint32_t val);

        /** The actual resource. */
        XftColor col;
    };
    /** Table: names -> colors. */
    extern std::unordered_map<std::string, XftColorWrapper> colors;
    /** Add a 0xRRGGBB formatted color to the global color table. */
    void add_color(std::string const& name, uint32_t val);
    /** Get a color from the table. */
    XftColorWrapper *get_color(std::string const& name);
    /** Convenience class enabling easily referring to colors by name. */
    struct Color {
        /** Load a color from the table. */
        Color(std::string const& name);
        Color(char const *name);

        XftColorWrapper *wrap;
    };


    /** Wrapper around an XftFont. */
    struct XftFontWrapper {
        /** Invalid. */
        XftFontWrapper();
        /** Load using XftFontOpenName (e.g. spec = "font:size=12"). */
        XftFontWrapper(std::string const& spec);

        /** Copying is prohibited. */
        XftFontWrapper(XftFontWrapper const&) =delete;

        /** Moving is ok. */
        XftFontWrapper(XftFontWrapper&& f);

        /** Call XftFontClose. */
        ~XftFontWrapper();

        /** The actual resource. */
        XftFont *fnt;
    };
    /** Table: names -> fonts. */
    extern std::unordered_map<std::string, XftFontWrapper> fonts;
    /** Add a font to the table. */
    void add_font(std::string const& name, std::string const& spec);
    /** Get a font from the table. */
    XftFontWrapper *get_font(std::string const& name);
    /** Convenience class enabling easily referring to fonts by name. */
    struct Font {
        /** Load a font from the table. */
        Font(std::string const& name);
        Font(char const* name);

        XftFontWrapper *wrap;
    };

    /** Convenience text object. */
    class Text {
    public:
        /** Construct with the given font and color.
         *
         * u8str is encoded in utf-8. */
        Text(Font fnt, Color col, std::string const& u8str="");

        /** Set the text. */
        Text& operator=(std::string const& u8str);

        Font fnt; /** The font. */
        Color col; /** The color. */

        /** Draw centered. */
        void draw(Coord x) const;

    protected:
        std::u16string u16; /** The text as utf16. */
    };

    /** Draw a solid-color background rectangle. */
    void fill_back(Coord x, Coord w, Color col);

    /** Flip the backbuffer. */
    void flip();

    /** Initialize the graphics system.
     *
     * Should be called before practically everything else. */
    void init();

    /** Xlib/Xft internals. */
    extern Display *dpy;
    extern int screen;
    extern Colormap cmap;
    extern Visual *vis;
    extern Window wnd, root;
    extern Pixmap backbuffer;
    extern XftDraw *xft_draw;
}

/** Interface containing general bar-related utilities. */
namespace bar {
    class Error {
        std::string msg;

    public:
        template<class... Args>
        Error(Args const&... args) {
            std::stringstream ss;
            using convert = int[];
            convert{0, ((void)(ss << args), 0)...};
            msg = ss.str();
        }
        friend Error operator+(Error const& a, Error const& b) {
            return Error(a.msg, "\n\t", b.msg);
        }
        friend std::ostream& operator<<(std::ostream& out, Error const& err) {
            out << err.msg;
            return out;
        }
    };

    using Event = XEvent; /** Event abstraction. */
    using EventType = int; /** As defined by XEvent. */
    using ETList = std::unordered_set<EventType>;
    /** We use Expose events as timed update events. */
    EventType const Update = Expose;
    /** Event that is sent only once, at start-up. */
    EventType const Startup = 1000;

    /** Interface for a bar component.
     *
     * Components are essentially listeners; when created, the bar asks them
     * which XEvent types they want to be notified of, and then when an event
     * of that type is encountered the event is forwarded to the relevant
     * components. */
    class Component {
    public:
        virtual ~Component();

        /** Handle an event. To be overriden by child classes. */
        virtual void update(Event const& ev) =0;

        /** Get which event types are relevant to this component. To be
         *  overriden by child classes. */
        virtual ETList get_relevant_event_types() const =0;
    };
    /** List of all bar components. */
    extern std::vector<std::unique_ptr<Component>> comps;

    /** Enter the event loop. */
    void run();
}

#endif // BAR_H_
