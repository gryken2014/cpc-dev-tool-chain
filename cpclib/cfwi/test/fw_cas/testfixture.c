#include "stdint.h"
#include "cfwi/cfwi.h"

#define NL "\015\012"

#define CPC_ARROW_RIGHT "\xf3"

#define hexchar(i) ( ( (i) < 10 ) ? ( '0' + (i) ) : ( 'A' - 10 + (i) ) )

void
print_uint8_as_hex( uint8_t v )
__z88dk_fastcall
{
        fw_txt_wr_char( hexchar( v >> 4 ) );
        fw_txt_wr_char( hexchar( v & 0x0F ) );
}

void
print_uint16_as_hex( uint16_t v )
__z88dk_fastcall
{
        print_uint8_as_hex( v >> 8 );
        print_uint8_as_hex( v );
}

void
decode_and_print_start_stop_motor_code( uint8_t code, uint8_t expectedcode )
{
        cfwi_txt_str0_output( CPC_ARROW_RIGHT " " );
        print_uint8_as_hex( code );
        cfwi_txt_str0_output( " " CPC_ARROW_RIGHT " " );
        cfwi_txt_str0_output( ( code & 1 ) ? "OK, " : "ESC," );
        cfwi_txt_str0_output( " was " );
        cfwi_txt_str0_output( ( code & 0x10 ) ? "ON  " : "OFF " );

        fw_txt_set_pen( ( code == expectedcode ) ? 2 : 3 );
        cfwi_txt_str0_output( ( code == expectedcode ) ? "PASS" : "FAIL" );
        fw_txt_set_pen( 1 );

        fw_txt_output( 13 );
        fw_txt_output( 10 );
}

void
test_start_stop_motor()
{
        cfwi_txt_str0_output( NL "CAS * MOTOR test, please wait." NL );

        cfwi_txt_str0_output( "START " );
        {
                uint8_t code = fw_cas_start_motor();

                decode_and_print_start_stop_motor_code( code, 0x01 );
        }
        cfwi_txt_str0_output( "START " );
        {
                uint8_t code = fw_cas_start_motor();

                decode_and_print_start_stop_motor_code( code, 0x11 );
        }
        cfwi_txt_str0_output( "STOP  " );
        {
                uint8_t code = fw_cas_stop_motor();

                decode_and_print_start_stop_motor_code( code, 0x11 );
        }
        cfwi_txt_str0_output( "STOP  " );
        {
                uint8_t code = fw_cas_stop_motor();

                decode_and_print_start_stop_motor_code( code, 0x01 );
        }

        cfwi_txt_str0_output( NL "Please press ESC NOW!" NL NL );

        cfwi_txt_str0_output( "START " );
        {
                uint8_t code = fw_cas_start_motor();

                decode_and_print_start_stop_motor_code( code, 0x00 );
        }
        fw_cas_stop_motor();
        cfwi_txt_str0_output( NL );
}

static const unsigned char const my_filename_in[] = "fwcas.bin";

enum
{
        my_filename_in_length = sizeof( my_filename_in ) - 1    // strip the
                                // null byte
                                // at end
};

const uint8_t my_buffer[2048];

fw_cas_in_open_parameters_t file_open_in_params =
{
        my_filename_in,
        my_filename_in_length,
        my_buffer
};

void
test_catalog()
{
        cfwi_txt_str0_output( NL "CAS CATALOG test, please wait." NL );

        {
                uint16_t rc = fw_cas_catalog( my_buffer );

                cfwi_txt_str0_output( NL "Return code: " );
                print_uint16_as_hex( rc );
                cfwi_txt_str0_output( " " CPC_ARROW_RIGHT " " );

                switch ( rc & 0xff )
                {
                        case 2:
                                cfwi_txt_str0_output( "(disc) Fail" NL );

                                {
                                        uint8_t errorbyte = rc >> 8;

                                        print_uint8_as_hex( rc );
                                }
                                break;

                        case 1:
                                cfwi_txt_str0_output( "ok" NL );
                                break;

                        case 0:
                                cfwi_txt_str0_output( "stream is in use" NL );

                                {
                                        uint8_t errorbyte = rc >> 8;

                                        print_uint8_as_hex( rc );
                                }
                                break;

                        default:
                                cfwi_txt_str0_output( "unexpected value" NL );
                }
        }
}

void
test_open_file_in()
{
        cfwi_txt_str0_output( NL "CAS IN OPEN test, please wait." NL );
        cfwi_txt_str0_output( "Will open file: " );
        cfwi_txt_str0_output( my_filename_in );
        cfwi_txt_str0_output( NL "Filename length: " );
        print_uint8_as_hex( my_filename_in_length );
        cfwi_txt_str0_output( NL NL );

        {
                uint8_t rc = fw_cas_in_open( &file_open_in_params );

                switch ( rc )
                {
                        case 2:
                                cfwi_txt_str0_output( "User hit escape" NL );
                                // user hit escape
                                break;

                        case 1:
                                // opened ok
                                cfwi_txt_str0_output( "File opened OK" NL );
                                cfwi_txt_str0_output( NL "File type: " );
                                print_uint8_as_hex
                                ( file_open_in_params.out_filetype_or_error );
                                cfwi_txt_str0_output( "Data location: " );
                                print_uint16_as_hex( ( uint16_t )
                                                     file_open_in_params.
                                                     out_data_location );
                                cfwi_txt_str0_output( NL "File length: " );
                                print_uint16_as_hex( ( uint16_t )
                                                     file_open_in_params.
                                                     out_logical_file_length );
                                cfwi_txt_str0_output( NL "Header location: " );
                                print_uint16_as_hex( ( uint16_t )
                                                     file_open_in_params.out_header );
                                break;

                        case 0:
                                // stream is in use
                                cfwi_txt_str0_output( "Stream is in use" NL );
                                break;
                }
        }
}

static const unsigned char const my_filename_out[] = "mydata.bin";

enum
{
        my_filename_out_length = sizeof( my_filename_out ) - 1  // strip the
                                 // null byte
                                 // at end
};

fw_cas_out_open_parameters_t file_open_out_params =
{
        my_filename_out,
        my_filename_out_length,
        my_buffer
};

void
test_open_file_out()
{
        cfwi_txt_str0_output( NL "CAS OUT OPEN test, please wait." NL );
        cfwi_txt_str0_output( "Will create file: " );
        cfwi_txt_str0_output( my_filename_out );
        cfwi_txt_str0_output( NL "Filename length: " );
        print_uint8_as_hex( my_filename_out_length );
        cfwi_txt_str0_output( NL );

        {
                uint8_t rc = fw_cas_out_open( &file_open_out_params );

                switch ( rc )
                {
                        case 2:
                                cfwi_txt_str0_output( "CANCELED: user hit escape" NL );
                                // user hit escape
                                break;

                        case 1:
                                // opened ok
                                cfwi_txt_str0_output( "SUCCEEDED." );
                                cfwi_txt_str0_output( NL "Header location: " );
                                print_uint16_as_hex( ( uint16_t )
                                                     file_open_out_params.out_header );
                                break;

                        case 0:
                                // stream is in use
                                cfwi_txt_str0_output( "CANCELED: Stream is in use"
                                                      NL );
                                break;
                }
        }
}

uint8_t
perform_test()
{
        fw_scr_set_ink( 2, 18, 18 );

        test_catalog();
        test_open_file_out();
        test_open_file_in();

        cfwi_txt_str0_output( "CAS INITIALISE" NL );
        fw_cas_initialise();

        cfwi_txt_str0_output( "CAS SET SPEED to 2000 bauds." NL );
        fw_cas_set_speed( 167, 50 );

        {
                int loop = 3;

                while ( loop-- > 0 )
                {
                        test_start_stop_motor();

                }
        }
        return 0;
}
