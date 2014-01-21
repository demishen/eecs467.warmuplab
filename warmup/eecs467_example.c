#include "eecs467_util.h"    // This is where a lot of the internals live

// It's good form for every application to keep its state in a struct.
typedef struct state state_t;
struct state
{
    char *url;
    int running;

    vx_application_t  vxapp;
    getopt_t         *gopt;
    parameter_gui_t  *pg;

    vx_world_t *world;  // Where vx objects are live

    zhash_t *layers;

    pthread_mutex_t mutex;  // for accessing the arrays
    pthread_t animate_thread;
};

// === Parameter listener =================================================
// This function is handled to the parameter gui (via a parameter listener)
// and handles events coming from the parameter gui. The parameter listener
// also holds a void* pointer to "impl", which can point to a struct holding
// state, etc if need be.
void my_param_changed(parameter_listener_t *pl, parameter_gui_t *pg, const char *name)
{
    if (!strcmp("sl1", name)) {
        printf("sl1 = %f\n", pg_gd(pg, name));
    } else if (!strcmp("sl2", name)) {
        printf("sl2 = %d\n", pg_gi(pg, name));
    } else if (!strcmp("cb1", name) | !strcmp("cb2", name)) {
        printf("%s = %d\n", name, pg_gb(pg, name));
    } else {
        printf("%s changed\n", name);
    }
}

// === Your code goes here ================================================
// The render loop handles your visualization updates. It is the function run
// by the animate_thread. It periodically renders the contents on the
// vx world contained by state
void* render_loop(void *data)
{
    int fps = 60;
    state_t *state = data;

    // Set up the imagesource
    image_source_t *isrc = image_source_open(state->url);

    if (isrc == NULL) {
        printf("Error opening device.\n");
    } else {
        // Print out possible formats. If no format was specified in the
        // url, then format 0 is picked by default.
        // e.g. of setting the format parameter to format 2:
        //
        // --url=dc1394://bd91098db0as9?fidx=2
        for (int i = 0; i < isrc->num_formats(isrc); i++) {
            image_source_format_t ifmt;
            isrc->get_format(isrc, i, &ifmt);
            printf("%3d: %4d x %4d (%s)\n", i, ifmt.width, ifmt.height, ifmt.format);
        }
        isrc->start(isrc);
    }

    // Continue running until we are signaled otherwise. This happens
    // when the window is closed/Ctrl+C is received.
    // while (state->running) {

    // Get the most recent camera frame and render it to screen.
    if (isrc != NULL) {
        image_source_data_t * frmd = calloc(1, sizeof(image_source_data_t));
        int res = isrc->get_frame(isrc, frmd);
        if (res < 0) {
            printf("get_frame fail: %d\n", res);
        } else {
            // Handle frame
            image_u32_t *im = image_u32_create_from_pnm("Waldo_search.ppm");
            image_u32_t *template = image_u32_create_from_pnm("Waldo_template.ppm");

            vx_object_t *vim = vxo_image_from_u32(im,
                                                  VXO_IMAGE_FLIPY,
                                                  VX_TEX_MIN_FILTER | VX_TEX_MAG_FILTER);


            vx_object_t *vtemplate = vxo_image_from_u32(template,
                                                  VXO_IMAGE_FLIPY,
                                                  VX_TEX_MIN_FILTER | VX_TEX_MAG_FILTER);


            double error = 0;
            double min_error = 10000000;
            int min_x, min_y;
            time_t now = time(0);
            int br = 0;
            now = time(0);
            printf(ctime(&now));
            printf("enter loop \n");
            for(int image_offset_x = 0; image_offset_x < im->width-template->width; image_offset_x++) {
                for(int image_offset_y = 0; image_offset_y < im->height-template->height; image_offset_y++) {
                    for (int ty = 0; ty < template->height; ty++) {
                        for (int tx = 0; tx < template->width; tx++) {
                            int t_idx = ty*template->stride + tx;
                            int im_idx = (image_offset_y+ty)*im->stride + (image_offset_x+tx);
                            int template_abgr = template->buf[t_idx];
                            int image_abgr = im->buf[im_idx];

                            int template_red = (template_abgr >> 0)&0xff;
                            int image_red = (image_abgr >> 0)&0xff;
                            // int template_green = (template_abgr >> 8)&0xff;
                            // int image_green = (image_abgr >> 8)&0xff;
                            int template_blue = (template_abgr >> 16)&0xff;
                            int image_blue = (image_abgr >> 16)&0xff;

                            error +=  sqrt(pow(template_red - image_red,2) +
                                           
                                           pow(template_blue - image_blue,2));
                        }
                    }
                    if(error < min_error) {
                        min_error = error;
                        min_x = image_offset_x;
                        min_y = image_offset_y;
                        
                    }
                    error = 0;
                }
                
            }
            now = time(0);
            printf(ctime(&now));
            // min_x = 383;
            // min_y = 80;
            // min_error = 0;

            printf("min x: %d, min y: %d, error: %f\n", min_x, min_y, min_error);
            printf("width: %d, height: %d\n", im->width, im->height);
            printf("temp width: %d, temp height: %d\n", template->width, template->height);


            // adjust display using vxo_chain
            vx_buffer_add_back(vx_world_get_buffer(state->world, "image"),
                               vxo_chain(vxo_mat_translate3(-im->width/20,-im->height/20,0),
                                        vxo_mat_scale3(0.1,0.1,1),
                                         vim));

            // adding the red rectangle over the image
            vim = vxo_chain(vxo_mat_translate3((-im->width/2+min_x+template->width/2-5)/10,(im->height/2-min_y-template->height/2+8)/10, 0),
                            vxo_mat_scale3(template->width/10, template->height/10, 1),
                            vxo_rect(vxo_lines_style(vx_red, 2.0f)));

            vx_buffer_add_back(vx_world_get_buffer(state->world, "image"), vim);


            vx_buffer_swap(vx_world_get_buffer(state->world, "image"));
            image_u32_destroy(im);
            image_u32_destroy(template);

        }
        fflush(stdout);
        // isrc->release_frame(isrc, frmd);
    }


    if (isrc != NULL)
        isrc->stop(isrc);

    return NULL;
}

// This is intended to give you a starting point to work with for any program
// requiring a GUI. This handles all of the GTK and vx setup, allowing you to
// fill in the functionality with your own code.
int main(int argc, char **argv)
{
    eecs467_init(argc, argv);

    state_t *state = calloc(1, sizeof(state_t));
    state->world = vx_world_create();

    state->vxapp.display_started = eecs467_default_display_started;
    state->vxapp.display_finished = eecs467_default_display_finished;
    state->vxapp.impl = eecs467_default_implementation_create(state->world);

    state->running = 1;

    // Parse arguments from the command line, showing the help
    // screen if required
    state->gopt = getopt_create();
    getopt_add_bool(state->gopt, 'h', "help", 0, "Show help");
    getopt_add_string(state->gopt, '\0', "url", "", "Camera URL");

    if (!getopt_parse(state->gopt, argc, argv, 1) || getopt_get_bool(state->gopt, "help"))
    {
        printf("Usage: %s [--url=CAMERAURL] [other options]\n\n", argv[0]);
        getopt_do_usage(state->gopt);
        exit(1);
    }

    // Set up the imagesource. This looks for a camera url specified on
    // the command line and, if none is found, enumerates a list of all
    // cameras imagesource can find and picks the first url it fidns.
    if (strncmp(getopt_get_string(state->gopt, "url"), "", 1)) {
        state->url = strdup(getopt_get_string(state->gopt, "url"));
        printf("URL: %s\n", state->url);
    } else {
        // No URL specified. Show all available and then
        // use the first

        zarray_t *urls = image_source_enumerate();
        printf("Cameras:\n");
        for (int i = 0; i < zarray_size(urls); i++) {
            char *url;
            zarray_get(urls, i, &url);
            printf("  %3d: %s\n", i, url);
        }

        if (zarray_size(urls) == 0) {
            printf("Found no cameras.\n");
            return -1;
        }

        zarray_get(urls, 0, &state->url);
    }

    // Initialize this application as a remote display source. This allows
    // you to use remote displays to render your visualization. Also starts up
    // the animation thread, in which a render loop is run to update your
    // display.
    vx_remote_display_source_t *cxn = vx_remote_display_source_create(&state->vxapp);


    pthread_create(&state->animate_thread, NULL, render_loop, state);

    // Initialize a parameter gui
    parameter_gui_t *pg = pg_create();
    
    parameter_listener_t *my_listener = calloc(1,sizeof(parameter_listener_t*));
    my_listener->impl = NULL;
    my_listener->param_changed = my_param_changed;
    pg_add_listener(pg, my_listener);

    state->pg = pg;

    eecs467_gui_run(&state->vxapp, state->pg, 800, 600);
    // Quit when GTK closes
    state->running = 0;

    pthread_join(state->animate_thread, NULL);
    vx_remote_display_source_destroy(cxn);

    // Cleanup
    //free(my_listener);

    vx_global_destroy();
    getopt_destroy(state->gopt);
}
