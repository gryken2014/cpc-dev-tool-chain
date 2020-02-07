#include <png.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include <argp.h>
#include <stdbool.h>

const char *argp_program_version = "png2cpcsprite 0.1";
const char *argp_program_bug_address = "<stephane_cpcitor@gourichon.org>";

#define symbol_format_string_default "sprite_%s"
#define module_format_string_default "module_%s"

static char doc[] =
        "\n"
        "png2cpcsprite by Stéphane Gourichon (cpcitor).\n"
        "\n"
        "Convert a PNG image into a binary representation suitable for "
        "display by an Amstrad CPC hardware or emulator, expressed as assembly "
        "source code.\n"
        "\n"
        "It is intended to be part of an automated worfklow/build from source "
        "assets in editable format to your ready-to-run-on-CPC production "
        "(demo, game), for example it is integrated in "
        "https://github.com/cpcitor/cpc-dev-tool-chain.\n"
        "\n"
        "The output is an assembly source text file with metadata available as "
        "symbols: byte_width, pixel_width, height, byte_size, and crtc_mode.\n"
        "\n"
        "Notice:\n"
        "* The input PNG file must be colormap-based.\n"
        "* When not specifying a colormap on the command-line, the actual "
        "palette is ignored by this program, only colormap index of each pixel "
        "is used.";
// static char args_doc[] = "[FILENAME]...";
static struct argp_option options[] = {
        {0, 0, 0, 0, "Input/output", 1},
        {"input", 'i', "<input_filename.png>", 0,
         "Path to an input file in PNG format with a palette (colormap).", 1},
        {"output", 'o', "<output_filename.s>", 0,
         "Path where the output file will be written in assembly source "
         "format.",
         1},
        {0, 0, 0, 0, "Processing", 2},
        {"palette", 'p', "colorcode[,colorcode]*", 0,
         "Optional.  "
         "If the runtime palette is known, specify it here, as comma-separated "
         "decimal values (same values as firmware/BASIC) like -p 1,24,20,6 or "
         "as 012 RGB triplets like -p 001,220,022,020 .  "
         "This option is not intended to perform general purpose color "
         "reduction but mostly to cope with source images that already conform "
         "to the intended palette yet in incorrect order or when image "
         "colormap has extra unwanted/unused colors.  "
         "To cancel a previous palette declaration, declare an empty string: "
         "-p ''.",
         2},
        {"mode", 'm', "<cpc-mode>", 0,
         "Optional.  "
         "CPC-mode 0, 1 or 2.  "
         "If unspecified or '-' the mode will be guessed from the size of the "
         "palette supplied on command-line, else the number of colormap "
         "entries in the input image."
         "In the latter case, make sure that your "
         "image doesn't include extra unused colormap entries which would "
         "confuse the very simple guessing logic.",
         2},
        {"direction", 'd', "<t> or <b>", 0,
         "Optional.  "
         "Default 't' is to write sprite data top to bottom. 'b' causes "
         "processing bottom to top.  Correct value depend on your context, "
         "especially sprite write routine.",
         2},
        {0, 0, 0, 0, "Assembly-level naming", 3},
        {"name_stem", 'n', "somename", 0,
         "Optional.  "
         "String associated with the image.  Think like the name of your image "
         "at assembly level.  "
         "Default is to generate a name from the file part in the 'input' "
         "argument, replacing invalid characters with an underscore '_'.",
         3},
        {"symbol_format_string", 1, "<myprefix_%s_mysuffix>", 0,
         "Optional.  "
         "Format string to generate symbol names.  "
         "A '%s' is mandatory else the generated assembly file will be "
         "invalid.  "
         "Default is '" symbol_format_string_default "'.",
         3},
        {"module_format_string", 2,
         "<myprefix_%s_mysuffix> or <my_module_name>", 0,
         "Optional.  "
         "Format string to generate an assembly module name.  "
         "It is okay to not include a '%s' so that several generated source "
         "files belong to the same module.  "
         "Default is '" module_format_string_default "'.",
         3},
        {0}};

#define MAX_EXPLICIT_PALETTE_COUNT 27

struct arguments
{
        char *input_file;
        char *output_file;
        bool crtc_mode_explicitly_set;
        u_int8_t crtc_mode;
        bool bottom_to_top;
        char *name_stem;
        char *symbol_format_string;
        char *module_format_string;
        unsigned int explicit_palette[MAX_EXPLICIT_PALETTE_COUNT];
        int explicit_palette_count;
        int verbose;
};

typedef struct byte_triplet
{
        u_int8_t r;
        u_int8_t g;
        u_int8_t b;
} byte_triplet;

// Todo take more precise RGB values from
// http://grimware.org/doku.php/documentations/devices/gatearray#inkr.color-codes
byte_triplet cpc_palette[27] = {
        {0, 0, 0},     {0, 0, 128},     {0, 0, 255},
        {128, 0, 0},   {128, 0, 128},   {128, 0, 255},
        {255, 0, 0},   {255, 0, 128},   {255, 0, 255},

        {0, 128, 0},   {0, 128, 128},   {0, 128, 255},
        {128, 128, 0}, {128, 128, 128}, {128, 128, 255},
        {255, 128, 0}, {255, 128, 128}, {255, 128, 255},

        {0, 255, 0},   {0, 255, 128},   {0, 255, 255},
        {128, 255, 0}, {128, 255, 128}, {128, 255, 255},
        {255, 255, 0}, {255, 255, 128}, {255, 255, 255}};

/*
Values from
http://grimware.org/doku.php/documentations/devices/gatearray#inkr.color-codes

#000201 00026B 0C02F4 6C0201 690268 6C02F2 F30506 F00268 F302F4 027801
#007868 0C7BF4 6E7B01 6E7D6B 6E7BF6 F37D0D F37D6B FA80F9 02F001 00F36B
#0FF3F2 71F504 71F36B 71F3F4 F3F30D F3F36D FFF3F9

and the extra ones #6E7B6D #F30268 #F3F36B #000268 #02F36B

sed 's/#\(..\)\(..\)\(..\)/ { 0x\1, 0x\2, 0x\3 }, /'
*/

byte_triplet cpc_palette_grimware_voltages[27] = {
        {0x00, 0x02, 0x01}, {0x00, 0x02, 0x6B}, {0x0C, 0x02, 0xF4},
        {0x6C, 0x02, 0x01}, {0x69, 0x02, 0x68}, {0x6C, 0x02, 0xF2},
        {0xF3, 0x05, 0x06}, {0xF0, 0x02, 0x68}, {0xF3, 0x02, 0xF4},
        {0x02, 0x78, 0x01}, {0x00, 0x78, 0x68}, {0x0C, 0x7B, 0xF4},
        {0x6E, 0x7B, 0x01}, {0x6E, 0x7D, 0x6B}, {0x6E, 0x7B, 0xF6},
        {0xF3, 0x7D, 0x0D}, {0xF3, 0x7D, 0x6B}, {0xFA, 0x80, 0xF9},
        {0x02, 0xF0, 0x01}, {0x00, 0xF3, 0x6B}, {0x0F, 0xF3, 0xF2},
        {0x71, 0xF5, 0x04}, {0x71, 0xF3, 0x6B}, {0x71, 0xF3, 0xF4},
        {0xF3, 0xF3, 0x0D}, {0xF3, 0xF3, 0x6D}, {0xFF, 0xF3, 0xF9},

        /* { 0x6E, 0x7B, 0x6D },
         { 0xF3, 0x02, 0x68 },
         { 0xF3, 0xF3, 0x6B },
         { 0x00, 0x02, 0x68 },
         { 0x02, 0xF3, 0x6B },  */
};

// From http://www.cpcwiki.eu/index.php/CPC_Palette
// Thanks http://www.cpcwiki.eu/index.php/User:MacDeath
u_int8_t firmware_colors[27] = {0x54, 0x44, 0x55, 0x5C, 0x58, 0x5D, 0x4C,
                                0x45, 0x4D, 0x56, 0x46, 0x57, 0x5E, 0x40,
                                0x5F, 0x4E, 0x47, 0x4F, 0x52, 0x42, 0x53,
                                0x5A, 0x59, 0x5B, 0x4A, 0x43, 0x4B};

/* Parse a single option. */
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
        /* Get the input argument from argp_parse, which we
           know is a pointer to our arguments structure. */
        struct arguments *arguments = state->input;

        char *reason = NULL;

        switch (key)
        {
        case ARGP_KEY_INIT:
                return 0;
        case ARGP_KEY_NO_ARGS:
                return 0;
        case ARGP_KEY_END:
                if (arguments->input_file == NULL)
                {
                        reason = "missing input file";
                        goto invalid;
                }
                if (arguments->output_file == NULL)
                {
                        reason = "missing output file";
                        goto invalid;
                }

                return 0;
        case ARGP_KEY_SUCCESS:
                return 0;
        case ARGP_KEY_FINI:
                return 0;
        case ARGP_KEY_ARG:
                reason = "stray argument";
                goto invalid;
                break;

        default:
                break;
        }

        /* fprintf(stderr, "key=%08xu '%c'\t", key, key); */

        printf("- argument '%s'\t... ", arg);
        switch (key)
        {
        case 1: /* symbol_format_string */
                arguments->symbol_format_string = arg;
                goto ok;
                break;
        case 2: /* module_format_string */
                arguments->module_format_string = arg;
                goto ok;
                break;
        case 'n': /* name_stem */
                  /* This option is handled here, not with others below,
                   * because an empty value is a correct value. */
                arguments->name_stem = arg;
                goto ok;
                break;
        case 'p':
        {
                unsigned int *palette_write_slot = arguments->explicit_palette;

                char *crp = arg;

                unsigned char c;

                unsigned int value_assuming_base3 = 0;
                bool value_assuming_base3_may_be_valid = true;
                unsigned int value_assuming_decimal = 0;
                unsigned int char_count_in_this_number = 0;

                arguments->explicit_palette_count = 0;

                if (*crp == 0)
                {
                        // -p '' removes any palette declaration.
                        goto ok;
                        break;
                }

                while (true)
                {
                        c = *(crp++);
                        /* printf("\n\tnext char (ascii %u) '%c';\n", c, c); */
                        if (c == ',' || c == 0)
                        {
                                /* printf("char_count_in_this_number = %d\n", */
                                /*        char_count_in_this_number); */
                                if (char_count_in_this_number == 3 &&
                                    value_assuming_base3_may_be_valid)
                                {
                                        /* printf("WRITING from base-3 decoded,
                                         * " */
                                        /*        "value is %u\n", */
                                        /*        value_assuming_base3); */
                                        *(palette_write_slot++) =
                                                value_assuming_base3;
                                }
                                else if (value_assuming_decimal <= 26)
                                {
                                        /* printf("WRITING from base-10 decoded,
                                         * " */
                                        /*        "value is %u\n", */
                                        /*        value_assuming_decimal); */
                                        *(palette_write_slot++) =
                                                value_assuming_decimal;
                                }
                                else
                                {
                                        fprintf(stderr,
                                                "Cannot parse valid ink number "
                                                "(neither base-3 nor decimal), "
                                                "aborting just before comma, "
                                                "at character %d of string "
                                                "'%s'\n",
                                                (int)(crp - arg - 1), arg);
                                        goto invalid;
                                }

                                // Okay, ink is valid.  Move on.
                                arguments->explicit_palette_count++;
                                value_assuming_base3 = 0;
                                value_assuming_base3_may_be_valid = true;
                                value_assuming_decimal = 0;
                                char_count_in_this_number = 0;

                                if (c != 0)
                                {
                                        if (arguments->explicit_palette_count ==
                                            16)
                                        {
                                                fprintf(stderr,
                                                        "Already parsed 16 "
                                                        "colours and still "
                                                        "something to parse? "
                                                        "Please check for "
                                                        "extraneous character "
                                                        "after your palette "
                                                        "declaration.\n");
                                                exit(1);
                                        }
                                        continue;
                                }
                                break;
                        }

                        if ((c >= '0') && (c <= '2') &&
                            (char_count_in_this_number <= 2))
                        {
                                unsigned int figure = c - '0';

                                unsigned int values_per_position[] = {3, 9, 1};

                                value_assuming_base3 +=
                                        figure *
                                        values_per_position
                                                [char_count_in_this_number];
                                /* printf("updating value_assuming_base3 to
                                 * %d\n", */
                                /*        value_assuming_base3); */
                        }
                        else
                        {
                                /* printf("value_assuming_base3_may_be_valid = "
                                 */
                                /*        "false;\n"); */
                                value_assuming_base3_may_be_valid = false;
                        }

                        if ((c >= '0') && (c <= '9'))
                        {
                                unsigned int figure = c - '0';
                                value_assuming_decimal =
                                        value_assuming_decimal * 10 + figure;
                                char_count_in_this_number++; // increase only
                                                             // here, we're
                                                             // going through
                                                             // this also in the
                                                             // base-3 case.
                                /* printf("updating value_assuming_decimal to "
                                 */
                                /*        "%d\n", */
                                /*        value_assuming_decimal); */
                                continue;
                        }

                        fprintf(stderr,
                                "Cannot parse palette: invalid character (not "
                                "figure or comma), aborting at character %d of "
                                "string '%s'\n",
                                (int)(crp - arg - 1), arg);
                        goto invalid;
                }
        }
                goto ok;
                break;

        default:
                break;
        }

        /* All arguments take non-empty parameters.  Reject if empty */
        if (arg[0] == 0)
        {
                reason = "empty string";
                goto invalid;
        }

        switch (key)
        {
        case 'i':
                arguments->input_file = arg;
                break;
        case 'o':
                arguments->output_file = arg;
                break;
        case 'd':
                // Assert only one character.
                if (arg[1] != 0)
                {
                        reason = "more than one character";
                        goto invalid;
                }

                if (arg[0] == 't')
                {
                        arguments->bottom_to_top = false;
                        break;
                }
                if (arg[0] == 'b')
                {
                        arguments->bottom_to_top = true;
                        break;
                }
                reason = "neither t nor b";
                goto invalid;
                break;

        case 'm':
                // Assert only one character.
                if (arg[1] != 0)
                {
                        reason = "more than one character";
                        goto invalid;
                }

                u_int8_t c = arg[0];

                switch (c)
                {
                case '0':
                case '1':
                case '2':
                        arguments->crtc_mode_explicitly_set = true;
                        arguments->crtc_mode = c - 48;
                        break;
                case '-':
                        arguments->crtc_mode_explicitly_set = false;
                        break;
                default:
                        reason = "neither 0,1,2,-";
                        goto invalid;
                        break;
                }

                break;
        default:
                return ARGP_ERR_UNKNOWN;
        }
ok:
        printf("ok\n");
        return 0;
invalid:
        argp_error(state, "Invalid argument (%s): '%s'", reason, arg);
        // Never reached.
        return ARGP_ERR_UNKNOWN;
}

/* Our argp parser. */
static struct argp argp = {options, parse_opt, 0 /* args_doc */, doc, 0, 0, 0};

u_int8_t guess_crtc_mode_based_on_colormap_entry_count(int colormap_entries)
{
        if (colormap_entries < 2)
        {
                fprintf(stderr, "Warning: less than 2 colors in colormap, "
                                "moving along anyway.\n");
        }
        if (colormap_entries == 2)
                return 2;
        if (colormap_entries <= 4)
                return 1;
        if (colormap_entries <= 16)
                return 0;
        return 4;
}

u_int8_t
find_palette_index_closest_to_this_rgb_triplet(struct arguments *arguments,
                                               u_int8_t *pixeldata)
{
        // Find index of color closest to this rgb triplet.
        // Compute distance to all colors in palette.
        // Take min.

        int pixel_r = pixeldata[0];
        int pixel_g = pixeldata[1];
        int pixel_b = pixeldata[2];

        unsigned int squared_distance_min = ~0;
        unsigned int squared_distance_min_index = ~0;

        for (int palette_index = 0;
             palette_index < arguments->explicit_palette_count; palette_index++)
        {
                int index_in_cpc_palette =
                        arguments->explicit_palette[palette_index];
                byte_triplet candidate_rgb = cpc_palette[index_in_cpc_palette];

                int dr = pixel_r - candidate_rgb.r;
                int dg = pixel_g - candidate_rgb.g;
                int db = pixel_b - candidate_rgb.b;

                unsigned int squared_distance = (dr * dr + dg * dg + db * db);

                if (squared_distance < squared_distance_min)
                {
                        squared_distance_min_index = palette_index;
                        squared_distance_min = squared_distance;

                        if (squared_distance == 0)
                        {
                                break;
                        }
                }
        }
        //  pr=%u pg=%u pb=%u
        /* fprintf(stderr, "r=%u g=%u b=%u index=%u/%u\n", pixel_r, pixel_g, */
        /*         pixel_b, squared_distance_min_index, */
        /*         arguments->explicit_palette_count); */

        return squared_distance_min_index;
}

#define maxargs 5
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

u_int8_t max_color_count_for_mode(u_int8_t m)
{
        return (1 << (1 << (2 - (m))));
}

int main(int argc, const char **argv)
{
        struct arguments arguments;
        memset(&arguments, 0, sizeof(arguments));
        arguments.symbol_format_string = symbol_format_string_default;
        arguments.module_format_string = module_format_string_default;

        /* Parse our arguments; every option seen by parse_opt will
           be reflected in arguments. */
        argp_parse(&argp, argc, (char **restrict)argv, 0, 0, &arguments);

        if (arguments.explicit_palette_count > 0)
        {
                printf("Explicit palette provided with %d entries:",
                       arguments.explicit_palette_count);

                for (int i = 0; i < arguments.explicit_palette_count; i++)
                {
                        printf(" %d", arguments.explicit_palette[i]);
                }
                printf("\n");
        }
        else
        {
                printf("Explicit palette not provided.\n");
        }

        png_image image;
        size_t buffer_size;

        png_bytep buffer_for_colormap;
        png_bytep buffer;

        {
                memset(&image, 0, (sizeof image));
                image.version = PNG_IMAGE_VERSION;
                image.opaque = NULL;

                {
                        printf("Will read from %s\n", arguments.input_file);

                        /* The first argument is the file to read: */
                        if (png_image_begin_read_from_file(
                                    &image, arguments.input_file) == 0)
                        {
                                fprintf(stderr, "png2cpcsprite: error: %s\n",
                                        image.message);
                                exit(1);
                        }
                }

                printf("Started decoding, found dimensions %u x %u, %u "
                       "colors, input libpng format code 0x%x.\n",
                       image.width, image.height, image.colormap_entries,
                       image.format);

                if (PNG_FORMAT_FLAG_ALPHA & image.format)
                {
                        fprintf(stderr,
                                "Warning: image format says it has "
                                "transparency.  "
                                "This programm cannot currently generate "
                                "sprites with transparent areas.  "
                                "For the sake of accepting this input I will "
                                "just assume that maybe you don't actually use "
                                "transparent or semi-transparent colors, and "
                                "ask the PNG decoder to just flatten partially "
                                "transparent areas assuming a black "
                                "background.  This may not be what you "
                                "want.\n");
                }

                image.format = PNG_FORMAT_RGB;

                // If no colormap is provided, will just pass the values
                // through.
                if (arguments.explicit_palette_count == 0)
                {
                        image.format |= PNG_FORMAT_FLAG_COLORMAP;
                }

                printf("Will decode with libpng format code 0x%x.\n",
                       image.format);

                buffer_size = PNG_IMAGE_SIZE(image);
                buffer = malloc(buffer_size);

                if (buffer == NULL)
                {
                        fprintf(stderr,
                                "png2cpcsprite: could not allocate %lu bytes "
                                "for image",
                                buffer_size);
                        // Yes, we don't cleanup.  Quick and dirty!
                        exit(1);
                }

                {
                        size_t colormap__size = PNG_IMAGE_COLORMAP_SIZE(image);
                        buffer_for_colormap = malloc(colormap__size);

                        if (buffer == NULL)
                        {
                                fprintf(stderr,
                                        "png2cpcsprite: could not allocate %lu "
                                        "bytes "
                                        "for colormap",
                                        colormap__size);
                                // Yes, we don't cleanup.  Quick and dirty!
                                exit(1);
                        }
                }

                png_color black = {0, 0, 0};

                if (png_image_finish_read(&image, &black, buffer,
                                          0 /*row_stride*/,
                                          buffer_for_colormap) == 0)
                {
                        fprintf(stderr, "png2cpcsprite: error: %s\n",
                                image.message);
                        // Yes, we don't cleanup.  Quick and dirty!
                        exit(1);
                }
        }

        printf("Finished decoding PNG. Processing.\n");

        if (!arguments.crtc_mode_explicitly_set)
        {
                printf("CRTC mode not determined by command line.\n");
                if (arguments.explicit_palette_count > 0)
                {
                        printf("Guessing from command-line colormap count (%u "
                               "entries).\n",
                               arguments.explicit_palette_count);
                        arguments.crtc_mode =
                                guess_crtc_mode_based_on_colormap_entry_count(
                                        arguments.explicit_palette_count);

                        if (arguments.crtc_mode == 4)
                        {
                                fprintf(stderr,
                                        "Internal error: "
                                        "guess_crtc_mode_based_on_colormap_"
                                        "entry_count returned an unexpected "
                                        "value.  Too many explicit palette "
                                        "entries (%u)?\n",
                                        arguments.explicit_palette_count);
                                exit(1);
                        }
                }
                else
                {
                        printf("Guessing from image colormap count (%u "
                               "entries).\n",
                               image.colormap_entries);
                        arguments.crtc_mode =
                                guess_crtc_mode_based_on_colormap_entry_count(
                                        image.colormap_entries);
                        if (arguments.crtc_mode == 4)
                        {
                                fprintf(stderr,
                                        "Error: the PNG palette has too many "
                                        "colors (%u) for the CPC, "
                                        "not trying to guess "
                                        "mode.  Please prepare the picture for "
                                        "the CPC.  In the "
                                        "special case where your picture is "
                                        "indeed prepared, actually "
                                        "uses the first indices of the palette "
                                        "only and only happens "
                                        "to have extraneous colormap entries "
                                        "at PNG level, set mode "
                                        "explicitly, for example: -mode 1 .\n",
                                        image.colormap_entries);
                                exit(1);
                        }
                }
        }

        unsigned int max_color_count_for_selected_mode =
                max_color_count_for_mode(arguments.crtc_mode);

        printf("CRTC mode selected: %u, which means a palette of %u colors.\n",
               arguments.crtc_mode, max_color_count_for_selected_mode);

        if (arguments.explicit_palette_count == 0)
        {
                printf("No palette provided on command line.  Assuming that "
                       "your nicely prepared your PNG with a "
                       "nice palette specially for the CPC.   Will map RGB "
                       "information from PNG image to CPC colors.\n");

                if (image.colormap_entries > max_color_count_for_selected_mode)
                {
                        fprintf(stderr,
                                "png2cpcsprite: Warning: colormap size is %u, "
                                "which is more than the %u, the maximum "
                                "allowed for CPC mode %u.  Since we are in "
                                "one-to-one "
                                "PNG-palette-index-to-CPC-palette-index mode, "
                                "this is okay if the image never uses palette "
                                "index %d or above, so moving along.\n",
                                image.colormap_entries,
                                max_color_count_for_selected_mode,
                                arguments.crtc_mode,
                                max_color_count_for_selected_mode);
                }

                u_int8_t *cmap_p = buffer_for_colormap;

                for (png_uint_32 cmap_i = 0; cmap_i < image.colormap_entries;
                     cmap_i++)
                {
                        // FIXME Should check if really 3 components? Or
                        // guaranteed by read parameters?
                        int png_cmap_r = *(cmap_p++);
                        int png_cmap_g = *(cmap_p++);
                        int png_cmap_b = *(cmap_p++);

                        unsigned int squared_distance_min = ~0;
                        unsigned int squared_distance_min_index = ~0;

                        for (int cpc_full_palette_index = 0;
                             cpc_full_palette_index < 27;
                             cpc_full_palette_index++)
                        {
                                byte_triplet candidate_rgb =
                                        cpc_palette[cpc_full_palette_index];

                                int dr = png_cmap_r - candidate_rgb.r;
                                int dg = png_cmap_g - candidate_rgb.g;
                                int db = png_cmap_b - candidate_rgb.b;

                                unsigned int squared_distance =
                                        (dr * dr + dg * dg + db * db);

                                if (squared_distance < squared_distance_min)
                                {
                                        squared_distance_min_index =
                                                cpc_full_palette_index;
                                        squared_distance_min = squared_distance;

                                        if (squared_distance == 0)
                                        {
                                                break;
                                        }
                                }
                        }

                        arguments.explicit_palette
                                [arguments.explicit_palette_count++] =
                                squared_distance_min_index;

                        printf("PNG palette entry %d (r,g,b)=(%u,%u,%u) mapped "
                               "to CPC color %u\n",
                               cmap_i, png_cmap_r, png_cmap_g, png_cmap_b,
                               squared_distance_min_index);
                }
        }

        unsigned int width_bytes = image.width >> (arguments.crtc_mode + 1);

        unsigned int width_pixels = width_bytes << (arguments.crtc_mode + 1);

        if (width_pixels != image.width)
        {
                fprintf(stderr,
                        "png2cpcsprite: Error: in the selected CPC mode %u, "
                        "image width %u pixels turns into %u bytes which will "
                        "expand to %u pixels, not %u.",
                        arguments.crtc_mode, image.width, width_bytes,
                        width_pixels, image.width);
                exit(1);
        }

        unsigned int sprite_bytes = width_bytes * image.height;

        printf("\nWill generate a sprite representation for CRTC mode %u, "
               "width "
               "%u pixels (%u bytes), height %u lines, total %u bytes.\n",
               arguments.crtc_mode, image.width, width_bytes, image.height,
               sprite_bytes);

        u_int8_t *sprite_buffer;
        {
                sprite_buffer = malloc(sprite_bytes);

                if (buffer == NULL)
                {
                        fprintf(stderr,
                                "png2cpcsprite: could not allocate %u bytes "
                                "for sprite buffer",
                                sprite_bytes);
                        // Yes, we don't cleanup.  Quick and dirty!
                        exit(1);
                }
        }

        if (arguments.crtc_mode != 1)
        {
                fprintf(stderr,
                        "png2cpcsprite: error: only mode 1 supported at this "
                        "time, not %d.",
                        arguments.crtc_mode);
                // Yes, we don't cleanup.  Quick and dirty!
                exit(1);
        }

        {
                u_int8_t *pixeldata = buffer;
                u_int8_t *w = sprite_buffer;
                for (size_t counter = 0; counter < sprite_bytes; counter++)
                {
                        u_int8_t cpc_byte = 0;

                        // mode 1 only for now
                        for (int pixel_in_byte = 0; pixel_in_byte < 4;
                             pixel_in_byte++)
                        {
                                u_int8_t color_palette_index;

                                if (PNG_FORMAT_FLAG_COLORMAP & image.format)
                                {
                                        color_palette_index = *pixeldata;

                                        if (color_palette_index >=
                                            max_color_count_for_selected_mode)
                                        {
                                                fprintf(stderr,
                                                        "Error: at pixel "
                                                        "number %lu, image "
                                                        "uses "
                                                        "palette index %d "
                                                        "which is too high "
                                                        "(>=%u) for this mode "
                                                        "of operation "
                                                        "(straight "
                                                        "PNG-palette-index-to-"
                                                        "CPC-palette-index) "
                                                        "and CPC mode %d.  "
                                                        "Result will most "
                                                        "certainly be ugly.  "
                                                        "Please prepare your "
                                                        "image for the CPC "
                                                        "beforehand.  "
                                                        "Aborting.\n",
                                                        counter,
                                                        color_palette_index,
                                                        max_color_count_for_selected_mode,
                                                        arguments.crtc_mode);
                                                exit(1);
                                        }

                                        pixeldata++;
                                }
                                else
                                {
                                        color_palette_index =
                                                find_palette_index_closest_to_this_rgb_triplet(
                                                        &arguments, pixeldata);
                                        pixeldata += PNG_IMAGE_SAMPLE_SIZE(
                                                image.format);
                                }

                                cpc_byte = cpc_byte << 1;
                                cpc_byte |= (color_palette_index & 2) >> 1 |
                                            ((color_palette_index & 1) << 4);
                        }

                        *w = cpc_byte;
                        w++;
                }

                if (pixeldata != buffer + buffer_size)
                {
                        fprintf(stderr,
                                "png2cpcsprite: warning: did not consume "
                                "exacly all %lu bytes of input buffer, "
                                "actually %lu (%p != %p).\n",
                                buffer_size, w - buffer, w,
                                buffer + buffer_size);
                }

                if (w != sprite_buffer + sprite_bytes)
                {
                        fprintf(stderr,
                                "png2cpcsprite: warning: did not produce "
                                "exacly the expected %u bytes of sprite data, "
                                "actually %lu (%p != %p).\n",
                                sprite_bytes, w - sprite_buffer, w,
                                sprite_buffer + sprite_bytes);
                }
        }

        printf("\nGenerated %u bytes of sprite data, will write them to output "
               "file '%s'.\n",
               sprite_bytes, arguments.output_file);

        if (!arguments.name_stem)
        {
                printf("No name stem supplied on command line.\n");

                const char *last_part_of_input_file_name =
                        strrchr(arguments.input_file, '/');
                if (last_part_of_input_file_name == NULL)
                {
                        last_part_of_input_file_name = arguments.input_file;
                }
                else
                {
                        last_part_of_input_file_name++;
                }

                char *auto_name_stem = strdup(last_part_of_input_file_name);

                /*
                  From
             cpc-dev-tool-chain/tool/sdcc/sdcc-3.9.0/sdas/doc/asmlnk.txt

             1.  Symbols  can  be  composed  of alphanumeric characters,
                 dollar signs ($),  periods  (.),  and  underscores  (_)
                 only.

             2.  The  first  character  of a symbol must not be a number
                 (except in the case of reusable symbols).

                 */

                char *auto_name_stem_end =
                        auto_name_stem + strlen(auto_name_stem);

                const char *valid_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghij"
                                          "klmnopqrstuvwxyz0123456789_";

                {
                        char *p = auto_name_stem;
                        while (p != auto_name_stem_end)
                        {
                                size_t advance = strspn(p, valid_chars);
                                p += advance;
                                if ((*p) != 0)
                                {
                                        *p = '_';
                                }
                        }
                }

                arguments.name_stem = auto_name_stem;
        }

        printf("Will use symbol name '%s'\n", arguments.name_stem);

        FILE *output_file = fopen(arguments.output_file, "w");

        if (output_file == NULL)
        {
                fprintf(stderr,
                        "png2cpcsprite: error: could not open output file "
                        "'%s'.",
                        arguments.output_file);
                // Yes, we don't cleanup.  Quick and dirty!
                exit(1);
        }

#define MAX_STRINGS_SIZE 255

        char symbol_name[MAX_STRINGS_SIZE];
        snprintf(symbol_name, MAX_STRINGS_SIZE, arguments.symbol_format_string,
                 arguments.name_stem);

        char module_name[MAX_STRINGS_SIZE];
        snprintf(module_name, MAX_STRINGS_SIZE, arguments.module_format_string,
                 arguments.name_stem);

        fprintf(output_file, ".module %s\n\n", module_name);

        fprintf(output_file, "%s_bytes == 0x%04x\n", symbol_name, sprite_bytes);
        fprintf(output_file, "%s_height == %d\n", symbol_name, image.height);
        fprintf(output_file, "%s_pixels_per_line == %d\n", symbol_name,
                width_pixels);
        fprintf(output_file, "%s_bytes_per_line == %d\n", symbol_name,
                width_bytes);

        if (arguments.explicit_palette_count > 0)
        {
                fprintf(output_file, "\n%s_palette_count == %d\n", symbol_name,
                        arguments.explicit_palette_count);

                for (int i = 0; i < arguments.explicit_palette_count; i++)
                {
                        fprintf(output_file, "%s_palette_ink_%d == %d\n",
                                symbol_name, i, arguments.explicit_palette[i]);
                }
                printf("\n");
        }

        fprintf(output_file, "\n%s_data::\n", symbol_name);

        {
                u_int8_t *b = sprite_buffer;
                for (size_t yplain = 0; yplain < image.height; yplain++)
                {
                        u_int8_t y = arguments.bottom_to_top
                                             ? image.height - 1 - yplain
                                             : yplain;
                        u_int8_t bytes_on_this_line = 0;

                        b = &(sprite_buffer[width_bytes * y]);

                        for (size_t x = 0; x < width_bytes; x++)
                        {
                                u_int8_t byte = *(b++);

                                if (bytes_on_this_line >= 12)
                                {
                                        bytes_on_this_line = 0;
                                }

                                if (bytes_on_this_line == 0)
                                {
                                        fprintf(output_file, "\n\t.byte ");
                                }
                                else
                                {
                                        fprintf(output_file, ", ");
                                }
                                bytes_on_this_line++;

                                fprintf(output_file, "0x%02x", byte);
                        }
                }
                fprintf(output_file, "\n");
        }

        fclose(output_file);

        printf("Finished writing file '%s'.\n", arguments.output_file);

        printf("Success. Exiting.\n");

        exit(0);
}
