/* Sega Genesis/Mega Drive NTSC video filter */

/* md_ntsc 0.1.2 */
#ifndef MD_NTSC_HPP
#define MD_NTSC_HPP

#include "md_ntsc_config.h"

// MDP includes.
#include "mdp/mdp_fncall.h"
#include "mdp/mdp_render.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Image parameters, ranging from -1.0 to 1.0. Actual internal values shown
in parenthesis and should remain fairly stable in future versions. */
typedef struct md_ntsc_setup_t
{
	union {
		struct {
			/* Basic parameters */
			double hue;        /* -1 = -180 degrees     +1 = +180 degrees */
			double saturation; /* -1 = grayscale (0.0)  +1 = oversaturated colors (2.0) */
			double contrast;   /* -1 = dark (0.5)       +1 = light (1.5) */
			double brightness; /* -1 = dark (0.5)       +1 = light (1.5) */
			double sharpness;  /* edge contrast enhancement/blurring */
			
			/* Advanced parameters */
			double gamma;      /* -1 = dark (1.5)       +1 = light (0.5) */
			double resolution; /* image resolution */
			double artifacts;  /* artifacts caused by color changes */
			double fringing;   /* color artifacts caused by brightness changes */
			double bleed;      /* color bleed (color resolution reduction) */
		};
		double params[10];
	};
	
	float const* decoder_matrix; /* optional RGB decoder matrix, 6 elements */
	
	unsigned char* palette_out;  /* optional RGB palette out, 3 bytes per color */
} md_ntsc_setup_t;

/* Video format presets */
DLL_LOCAL extern md_ntsc_setup_t const md_ntsc_composite; /* color bleeding + artifacts */
DLL_LOCAL extern md_ntsc_setup_t const md_ntsc_svideo;    /* color bleeding only */
DLL_LOCAL extern md_ntsc_setup_t const md_ntsc_rgb;       /* crisp image */
DLL_LOCAL extern md_ntsc_setup_t const md_ntsc_monochrome;/* desaturated + artifacts */

enum { md_ntsc_palette_size = 512 };

/* Initializes and adjusts parameters. Can be called multiple times on the same
md_ntsc_t object. Can pass NULL for either parameter. */
typedef struct md_ntsc_t md_ntsc_t;
void md_ntsc_init( md_ntsc_t* ntsc, md_ntsc_setup_t const* setup );

/* Number of output pixels written by blitter for given input width. */
#define MD_NTSC_OUT_WIDTH( in_width ) \
	(((in_width) - 3) / md_ntsc_in_chunk * md_ntsc_out_chunk + md_ntsc_out_chunk)

/* Number of input pixels that will fit within given output width. Might be
rounded down slightly; use MD_NTSC_OUT_WIDTH() on result to find rounded
value. */
#define MD_NTSC_IN_WIDTH( out_width ) \
	((out_width) / md_ntsc_out_chunk * md_ntsc_in_chunk - md_ntsc_in_chunk + 3)


/* Interface for user-defined custom blitters */

enum { md_ntsc_in_chunk  = 4 }; /* number of input pixels read per chunk */
enum { md_ntsc_out_chunk = 8 }; /* number of output pixels generated per chunk */
enum { md_ntsc_black     = 0 }; /* palette index for black */

/* Begin outputting row and start three pixels. First pixel will be cut off a bit.
Use md_ntsc_black for unused pixels. Declares variables, so must be before first
statement in a block (unless you're using C++). */
#define MD_NTSC_BEGIN_ROW( ntsc, pixel0, pixel1, pixel2, pixel3 ) \
	unsigned const md_pixel0_ = (pixel0);\
	md_ntsc_rgb_t const* kernel0  = MD_NTSC_IN_FORMAT( ntsc, md_pixel0_ );\
	unsigned const md_pixel1_ = (pixel1);\
	md_ntsc_rgb_t const* kernel1  = MD_NTSC_IN_FORMAT( ntsc, md_pixel1_ );\
	unsigned const md_pixel2_ = (pixel2);\
	md_ntsc_rgb_t const* kernel2  = MD_NTSC_IN_FORMAT( ntsc, md_pixel2_ );\
	unsigned const md_pixel3_ = (pixel3);\
	md_ntsc_rgb_t const* kernel3  = MD_NTSC_IN_FORMAT( ntsc, md_pixel3_ );\
	md_ntsc_rgb_t const* kernelx0;\
	md_ntsc_rgb_t const* kernelx1 = kernel0;\
	md_ntsc_rgb_t const* kernelx2 = kernel0;\
	md_ntsc_rgb_t const* kernelx3 = kernel0

/* Begin input pixel */
#define MD_NTSC_COLOR_IN( index, ntsc, color ) \
	MD_NTSC_COLOR_IN_( index, color, MD_NTSC_IN_FORMAT, ntsc )

/* Generate output pixel. Bits can be 24, 16, 15, 32 (treated as 24), or 0:
24: RRRRRRRR GGGGGGGG BBBBBBBB
16:          RRRRRGGG GGGBBBBB
15:           RRRRRGG GGGBBBBB
 0: xxxRRRRR RRRxxGGG GGGGGxxB BBBBBBBx (native internal format; x = junk bits) */
#define MD_NTSC_RGB_OUT( x, rgb_out ) {\
	md_ntsc_rgb_t raw_ =\
		kernel0  [x+ 0] + kernel1  [(x+6)%8+16] + kernel2  [(x+4)%8  ] + kernel3  [(x+2)%8+16] +\
		kernelx0 [x+ 8] + kernelx1 [(x+6)%8+24] + kernelx2 [(x+4)%8+8] + kernelx3 [(x+2)%8+24];\
	MD_NTSC_CLAMP_( raw_, 0 );\
	MD_NTSC_RGB_OUT_<pixel, maskR, maskG, maskB, shiftR, shiftG, shiftB>( &rgb_out, 0, raw_ );\
}


/* private */
enum { md_ntsc_entry_size = 2 * 16 };
typedef unsigned long md_ntsc_rgb_t;
struct md_ntsc_t {
	md_ntsc_rgb_t table [md_ntsc_palette_size] [md_ntsc_entry_size];
};

#define MD_NTSC_BGR9( ntsc, n ) (ntsc)->table [n & 0x1FF]

#define MD_NTSC_RGB16( ntsc, n ) \
	(md_ntsc_rgb_t*) ((char*) (ntsc)->table +\
	((n << 9 & 0x3800) | (n & 0x0700) | (n >> 8 & 0x00E0)) *\
	(md_ntsc_entry_size * sizeof (md_ntsc_rgb_t) / 32))

/* common ntsc macros */
#define md_ntsc_rgb_builder    ((1L << 21) | (1 << 11) | (1 << 1))
#define md_ntsc_clamp_mask     (md_ntsc_rgb_builder * 3 / 2)
#define md_ntsc_clamp_add      (md_ntsc_rgb_builder * 0x101)
#define MD_NTSC_CLAMP_( io, shift ) {\
	md_ntsc_rgb_t sub = (io) >> (9-(shift)) & md_ntsc_clamp_mask;\
	md_ntsc_rgb_t clamp = md_ntsc_clamp_add - sub;\
	io |= clamp;\
	clamp -= sub;\
	io &= clamp;\
}

#define MD_NTSC_COLOR_IN_( index, color, ENTRY, table ) {\
	unsigned color_;\
	kernelx##index = kernel##index;\
	kernel##index = (color_ = (color), ENTRY( table, color_ ));\
}

/* MDP Renderer Functions. */
DLL_LOCAL int MDP_FNCALL mdp_md_ntsc_init(void);
DLL_LOCAL int MDP_FNCALL mdp_md_ntsc_end(void);

DLL_LOCAL int MDP_FNCALL mdp_md_ntsc_blit(const mdp_render_info_t *render_info);

/* NTSC setup struct. */
DLL_LOCAL void MDP_FNCALL mdp_md_ntsc_reinit_setup(void);
DLL_LOCAL extern md_ntsc_setup_t mdp_md_ntsc_setup;

/* Scanline / Interpolation options. */
#define MDP_MD_NTSC_EFFECT_SCANLINE	(1 << 0)
#define MDP_MD_NTSC_EFFECT_INTERP	(1 << 1)
#define MDP_MD_NTSC_EFFECT_CXA2025AS	(1 << 2)
DLL_LOCAL extern unsigned int mdp_md_ntsc_effects;

#ifdef __cplusplus
}
#endif

#endif /* MD_NTSC_HPP */
