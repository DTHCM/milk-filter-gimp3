#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <glib/grand.h>

#define PLUG_IN_PROC "plug-in-dthcm-milkify"

struct _Milk {
  GimpPlugIn parent_instance;
};

#define MILK_TYPE (milk_get_type())
G_DECLARE_FINAL_TYPE(Milk, milk, MILK, , GimpPlugIn)

static const char *authors = "AnalogFeelings, LucaSinUnaS, DTHCM";
static const char *date = "2025";

enum {
  STYLE_OUTSIDE,
  STYLE_INSIDE,
};

struct milk_args {
  gint style;
  gboolean punt;
};

#define MILK_ARG_DEFAULT { STYLE_OUTSIDE, 0 }

static void milk(GimpDrawable *drawable, const struct milk_args *args);

static gboolean milk_set_i18n(GimpPlugIn *plug_in, const gchar *procedure_name,
                              gchar **gettext_domain, gchar **catalog_dir);
static GList *milk_query_procedures(GimpPlugIn *plug_in);
static GimpProcedure *milk_create_procedure(GimpPlugIn *plug_in,
                                            const gchar *name);

static GimpValueArray *milk_run(GimpProcedure *procedure, GimpRunMode run_mode,
                                GimpImage *image, GimpDrawable **drawables,
                                GimpProcedureConfig *config, gpointer run_data);

G_DEFINE_TYPE(Milk, milk, GIMP_TYPE_PLUG_IN)

static void milk_class_init(MilkClass *klass) {
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS(klass);

  plug_in_class->set_i18n = milk_set_i18n;
  plug_in_class->query_procedures = milk_query_procedures;
  plug_in_class->create_procedure = milk_create_procedure;
}

static void milk_init(Milk *milk) {}

static gboolean milk_set_i18n(GimpPlugIn *plug_in, const gchar *procedure_name,
                              gchar **gettext_domain, gchar **catalog_dir) {
  return 0;
}

static GList *milk_query_procedures(GimpPlugIn *plug_in) {
  return g_list_append(NULL, g_strdup(PLUG_IN_PROC));
}

static GimpProcedure *milk_create_procedure(GimpPlugIn *plug_in,
                                            const gchar *name) {
  GimpProcedure *procedure = NULL;

  if (g_strcmp0(name, PLUG_IN_PROC) == 0)
    procedure = gimp_image_procedure_new(
        plug_in, name, GIMP_PDB_PROC_TYPE_PLUGIN, milk_run, NULL, NULL);
  gimp_procedure_set_menu_label(procedure, "Milkify");
  gimp_procedure_add_menu_path(procedure, "<Image>/Filters/Artistic");
  gimp_procedure_set_image_types(procedure, "RGB*");

  gimp_procedure_set_documentation(
      procedure, "Convert layer to milk game style.", NULL, NULL);
  gimp_procedure_set_attribution(procedure, authors, authors, date);

  gimp_procedure_add_choice_argument(
      procedure, "style", "Style", "Style to game.",
      gimp_choice_new_with_values("style-outside", STYLE_OUTSIDE, "Outside",
                                  NULL, "style-inside", STYLE_INSIDE, "Inside",
                                  NULL, NULL),
      "style-outside", G_PARAM_READWRITE);
  gimp_procedure_add_boolean_argument(procedure, "punt", "Puntilism",
                                      "Should the filter use puntilism.", FALSE,
                                      G_PARAM_READWRITE);

  return procedure;
}

static int rand70perc() { return g_random_int_range(0, 10000) < 7000; }
static int norand() { return 1; }

static GimpValueArray *milk_run(GimpProcedure *procedure, GimpRunMode run_mode,
                                GimpImage *image, GimpDrawable **drawables,
                                GimpProcedureConfig *config,
                                gpointer run_data) {
  GimpTextLayer *text_layer;
  GimpLayer *parent = NULL;
  gint position = 0;
  gint n_drawables;

  gchar *text;
  GimpFont *font;
  gint size;
  GimpUnit *unit;

  n_drawables = gimp_core_object_array_get_length((GObject **)drawables);

  if (n_drawables != 1) {
    GError *error = NULL;

    g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                "Procedure '%s' works with one layer.", PLUG_IN_PROC);

    return gimp_procedure_new_return_values(procedure, GIMP_PDB_CALLING_ERROR,
                                            error);
  }

  GimpDrawable *drawable = drawables[0];

  if (!GIMP_IS_LAYER(drawable)) {
    GError *error = NULL;

    g_set_error(&error, GIMP_PLUG_IN_ERROR, 0,
                "Procedure '%s' works with layers only.", PLUG_IN_PROC);

    return gimp_procedure_new_return_values(procedure, GIMP_PDB_CALLING_ERROR,
                                            error);
  }

  gegl_init (NULL, NULL);

  struct milk_args args = MILK_ARG_DEFAULT;

  if (run_mode == GIMP_RUN_INTERACTIVE) {
    GtkWidget *dialog;
    gimp_ui_init(PLUG_IN_PROC);
    dialog = gimp_procedure_dialog_new(procedure, GIMP_PROCEDURE_CONFIG(config),
                                       "Milkify");
    gimp_procedure_dialog_fill(GIMP_PROCEDURE_DIALOG(dialog), NULL);

    if (!gimp_procedure_dialog_run(GIMP_PROCEDURE_DIALOG(dialog)))
      return gimp_procedure_new_return_values(procedure, GIMP_PDB_CANCEL, NULL);

    gtk_widget_destroy (dialog);
  }

  args.style = gimp_procedure_config_get_choice_id(config, "style");
  g_object_get(config, "punt", &args.punt, NULL);

  milk(drawable, &args);

  return gimp_procedure_new_return_values(procedure, GIMP_PDB_SUCCESS, NULL);
}

static void milk(GimpDrawable *drawable, const struct milk_args *args) {
  GeglBuffer *src_buffer = gimp_drawable_get_buffer(drawable);
  // allows undo
  GeglBuffer *dest_buffer = gimp_drawable_get_shadow_buffer (drawable);

  const Babl *format;
  gint width, height;
  guchar *tmp, *iter;
  gint bytes;

  width = gegl_buffer_get_width(src_buffer);
  height = gegl_buffer_get_height(src_buffer);
  tmp = g_new(guchar, width * height * 4);
  iter = tmp;

  if (gimp_drawable_has_alpha(drawable))
    format = babl_format("R'G'B'A u8");
  else
    format = babl_format("R'G'B' u8");
  bytes = babl_format_get_bytes_per_pixel (format);

  gegl_buffer_get(src_buffer, GEGL_RECTANGLE(0, 0, width, height), 1.0, format, tmp,
                  GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);

  struct {
    unsigned char r, g, b;
  } palette_inside[] =
      {
          {0, 0, 0},
          {92, 36, 60},
          {203, 43, 43},
      },
    palette_outside[] = {
        {0, 0, 0},
        {102, 0, 31},
        {137, 0, 146},
    }, *palette, *c;

  palette = args->style == STYLE_INSIDE ? palette_inside : palette_outside;

  gint thresh_mid1 = args->style == STYLE_OUTSIDE ? 120 : 90;
  gint thresh_mid2 = args->style == STYLE_OUTSIDE ? 200 : 150;

  int (*randlt)() = args->punt ? rand70perc : norand;

  for (int i = 0; i < width * height; i++) {
    int brightness = iter[0] + iter[1] + iter[2];
    brightness /= 3;
    if (brightness <= 25) {
      c = &palette[0];
    } else if (brightness <= 70) {
      c = randlt() ? &palette[0] : &palette[1];
    } else if (brightness <= thresh_mid1) {
      c = randlt() ? &palette[1] : &palette[0];
    } else if (brightness <= thresh_mid2) {
      c = &palette[1];
    } else if (brightness < 230) {
      c = randlt() ? &palette[0] : &palette[1];
    } else {
      c = &palette[2];
    }
    iter[0] = c->r; iter[1] = c->g; iter[2] = c->b;
    iter += bytes;
  }

  gegl_buffer_set(dest_buffer, GEGL_RECTANGLE(0, 0, width, height), 0, format,
                  tmp, GEGL_AUTO_ROWSTRIDE);

  g_object_unref(dest_buffer);
  g_object_unref(src_buffer);
  g_free(tmp);

  gimp_drawable_merge_shadow(drawable, TRUE);
  gimp_drawable_update(drawable, 0, 0, width, height);
}

/* Generate needed main() code */
GIMP_MAIN(MILK_TYPE)
