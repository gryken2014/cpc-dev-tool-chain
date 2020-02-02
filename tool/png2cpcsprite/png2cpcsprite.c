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
        "display by an Amstrad CPC hardware or emulator.\n"
        "\n"
        "That binary representation is in turn written in a assembly source "
        "text file with metadata available as symbols: byte_width, "
        "pixel_width, height, byte_size, and crtc_mode.\n"
        "\n"
        "Notice:\n"
        "* The input PNG file must be colormap-based.\n"
        "* The actual palette is ignored by this program.";
// static char args_doc[] = "[FILENAME]...";
static struct argp_option options[] = {
        {0, 0, 0, 0, "Input/output", 1},
        {"input", 'i', "<input_filename.png>", 0,
         "Path to an input file in PNG format with a palette (colormap).", 1},
        {"output", 'o', "<output_filename.s>", 0,
         "Path where the output file will be written in assembly source "
         "format.",
         1},
        {0, 0, 0, 0, "Output format", 2},
        {"mode", 'm', "<cpc-mode>", 0,
         "CPC-mode 0, 1 or 2.  "
         "If unspecified or '-' the mode will be guessed from the number or "
         "colormap entries.  Make sure that your image doesn't include extra "
         "unused palette entries which would confuse the guessing logic.",
         2},
        {"direction", 'd', "<t> or <b>", 0,
         "Default 't' is to write sprite data top to bottom. 'b' causes "
         "processing bottom to top.  Correct value depend on your context, "
         "especially sprite write routine.",
         2},
        {0, 0, 0, 0, "Assembly-level naming", 3},
        {"name_stem", 'n', "somename", 0,
         "String associated with the image.  Think like the name of your image "
         "at assembly level.  "
         "Default is to generate a name from the file part in the 'input' "
         "argument, replacing invalid characters with an underscore '_'.",
         3},
        {"symbol_format_string", 1, "<myprefix_%s_mysuffix>", 0,
         "Format string to generate symbol names.  "
         "A '%s' is mandatory else the generated assembly file will be invalid."
         "Default is '" symbol_format_string_default "'.",
         3},
        {"module_format_string", 2,
         "<myprefix_%s_mysuffix> or <my_module_name>", 0,
         "Format string to generate an assembly module name.  "
         "It is okay to not include a '%s' so that several generated source "
         "files belong to the same module.  "
         "Default is '" module_format_string_default "'.",
         3},
        {0}};

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
        int verbose;
};

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

        fprintf(stderr, "key=%08xu '%c'\t", key, key);

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
                arguments->name_stem = arg;
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

png_bytep read_png(const char *const input_file_name, png_image *image,
                   size_t *buffer_size)
{
        memset(image, 0, (sizeof *image));
        image->version = PNG_IMAGE_VERSION;
        image->opaque = NULL;

        {
                printf("Will read from %s\n", input_file_name);

                /* The first argument is the file to read: */
                if (png_image_begin_read_from_file(image, input_file_name) == 0)
                {
                        fprintf(stderr, "png2cpcsprite: error: %s\n",
                                image->message);
                        exit(1);
                }
        }

        printf("Started decoding, found dimensions %u x %u, %u colors.\n",
               image->width, image->height, image->colormap_entries);

        png_bytep buffer, buffer_for_colormap;

        image->format = PNG_FORMAT_RGB_COLORMAP;

        *buffer_size = PNG_IMAGE_SIZE(*image);
        buffer = malloc(*buffer_size);

        if (buffer == NULL)
        {
                fprintf(stderr,
                        "png2cpcsprite: could not allocate %lu bytes "
                        "for image",
                        *buffer_size);
                // Yes, we don't cleanup.  Quick and dirty!
                exit(1);
        }

        {
                size_t colormap__size = PNG_IMAGE_COLORMAP_SIZE(*image);
                buffer_for_colormap = malloc(colormap__size);

                if (buffer == NULL)
                {
                        fprintf(stderr,
                                "png2cpcsprite: could not allocate %lu bytes "
                                "for colormap",
                                colormap__size);
                        // Yes, we don't cleanup.  Quick and dirty!
                        exit(1);
                }
        }

        if (png_image_finish_read(image, NULL /*background*/, buffer,
                                  0 /*row_stride*/, buffer_for_colormap) == 0)
        {
                fprintf(stderr, "png2cpcsprite: error: %s\n", image->message);
                // Yes, we don't cleanup.  Quick and dirty!
                exit(1);
        }

        return buffer;
}

u_int8_t guess_crtc_mode_based_on_colormap_entry_count(int colormap_entries)
{
        if (colormap_entries < 2)
        {
                fprintf(stderr, "Warning: strange image with less than 2 "
                                "colors in colormap, moving along anyway.\n");
        }
        if (colormap_entries == 2)
                return 2;
        if (colormap_entries <= 4)
                return 1;
        if (colormap_entries <= 16)
                return 0;
        fprintf(stderr, "Error: too many colors (%u) for the CPC, aborting.",
                colormap_entries);
        exit(1);
}

#define maxargs 5
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

int main(int argc, const char **argv)
{
        struct arguments arguments;
        arguments.symbol_format_string = symbol_format_string_default;
        arguments.module_format_string = module_format_string_default;

        /* Parse our arguments; every option seen by parse_opt will
           be reflected in arguments. */
        argp_parse(&argp, argc, (char **restrict)argv, 0, 0, &arguments);

        png_image image;
        size_t buffer_size;

        png_bytep buffer = read_png(arguments.input_file, &image, &buffer_size);

        printf("Finished decoding PNG. Processing.\n");

        if (!arguments.crtc_mode_explicitly_set)
        {
                printf("CRTC mode not determined by command line, guessing "
                       "from colormap count (%u entries).\n",
                       image.colormap_entries);
                arguments.crtc_mode =
                        guess_crtc_mode_based_on_colormap_entry_count(
                                image.colormap_entries);
        }

        printf("CRTC mode selected: %u.\n", arguments.crtc_mode);

        unsigned int width_bytes = image.width >> (arguments.crtc_mode + 1);

        unsigned int width_pixels = width_bytes << (arguments.crtc_mode + 1);

        if (width_pixels != image.width)
        {
                fprintf(stderr,
                        "png2cpcsprite: "
                        "Error: in the selected CPC mode %u, "
                        "image width %u "
                        "make around %u bytes "
                        "which will expand to %u pixels, not %u.",
                        arguments.crtc_mode, image.width, width_bytes,
                        width_pixels, image.width);
                exit(1);
        }

        unsigned int sprite_bytes = width_bytes * image.height;

        printf("Will generate a sprite representation for CRTC mode %u, "
               "width %u pixels (%u bytes), height %u lines, total %u bytes.\n",
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
                u_int8_t *r = buffer;
                u_int8_t *w = sprite_buffer;
                for (size_t counter = 0; counter < sprite_bytes; counter++)
                {
                        u_int8_t cpc_byte = 0;

                        // mode 1 only for now
                        for (int pixel_in_byte = 0; pixel_in_byte < 4;
                             pixel_in_byte++)
                        {
                                u_int8_t color_palette_index = *(r++);
                                cpc_byte = cpc_byte << 1;
                                cpc_byte |= (color_palette_index & 1) |
                                            ((color_palette_index & 2) << 3);
                        }

                        *w = cpc_byte;
                        w++;
                }

                if (r != buffer + buffer_size)
                {
                        fprintf(stderr,
                                "png2cpcsprite: warning: did not consume "
                                "exacly all %lu bytes "
                                "of input buffer, actually %lu (%p != %p).\n",
                                buffer_size, w - buffer, w,
                                buffer + buffer_size);
                }

                if (w != sprite_buffer + sprite_bytes)
                {
                        fprintf(stderr,
                                "png2cpcsprite: warning: did not produce "
                                "exacly the expected %u bytes "
                                "of sprite data, actually %lu (%p != %p).\n",
                                sprite_bytes, w - sprite_buffer, w,
                                sprite_buffer + sprite_bytes);
                }
        }

        printf("Generated %u bytes "
               "of sprite data, will write them to output file '%s'.\n",
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
                                          "klmnopqrstuvwxyz0123456789$._";

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

        fprintf(output_file, "%s_data::\n", symbol_name);

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
