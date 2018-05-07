/*-
 * Copyright (c) 2008      Jannis Pohlmann <jannis@xfce.org>
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo/exo.h>

#include "xfce-text-renderer.h"



enum
{
  PROP_0,
  PROP_FOLLOW_PRELIT,
  PROP_FOLLOW_STATE,
  PROP_TEXT,
  PROP_WRAP_MODE,
  PROP_WRAP_WIDTH,
};

enum
{
  EDITED,
  LAST_SIGNAL,
};



static void xfce_text_renderer_finalize     (GObject               *object);
static void xfce_text_renderer_get_property (GObject               *object,
                                             guint                  prop_id,
                                             GValue                *value,
                                             GParamSpec            *pspec);
static void xfce_text_renderer_set_property (GObject               *object,
                                             guint                  prop_id,
                                             const GValue          *value,
                                             GParamSpec            *pspec);
static void xfce_text_renderer_get_size     (GtkCellRenderer       *renderer,
                                             GtkWidget             *widget,
                                             GdkRectangle          *cell_area,
                                             gint                  *x_offset,
                                             gint                  *y_offset,
                                             gint                  *width,
                                             gint                  *height);
static void xfce_text_renderer_render       (GtkCellRenderer       *renderer,
                                             GdkWindow             *window,
                                             GtkWidget             *widget,
                                             GdkRectangle          *background_area,
                                             GdkRectangle          *cell_area,
                                             GdkRectangle          *expose_area,
                                             GtkCellRendererState   flags);
static void xfce_text_renderer_invalidate   (XfceTextRenderer      *text_renderer);
static void xfce_text_renderer_set_widget   (XfceTextRenderer      *text_renderer,
                                             GtkWidget             *widget);



struct _XfceTextRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _XfceTextRenderer
{
  GtkCellRenderer __parent__;

  PangoLayout  *layout;
  GtkWidget    *widget;
  gboolean      text_static;
  gchar        *text;
  gint          char_width;
  gint          char_height;
  PangoWrapMode wrap_mode;
  gint          wrap_width;
  gboolean      follow_state;
  gint          focus_width;;

  /* underline prelited rows */
  gboolean      follow_prelit;
};



G_DEFINE_TYPE (XfceTextRenderer, xfce_text_renderer, GTK_TYPE_CELL_RENDERER)



static void
xfce_text_renderer_class_init (XfceTextRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = xfce_text_renderer_finalize;
  gobject_class->get_property = xfce_text_renderer_get_property;
  gobject_class->set_property = xfce_text_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = xfce_text_renderer_get_size;
  gtkcell_renderer_class->render = xfce_text_renderer_render;

  /**
   * XfceTextRenderer:follow-prelit:
   *
   * Whether to underline prelited cells. This is used for the single
   * click support in the detailed list view.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLLOW_PRELIT,
                                   g_param_spec_boolean ("follow-prelit",
                                                         "follow-prelit",
                                                         "follow-prelit",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * XfceTextRenderer:follow-state:
   *
   * Specifies whether the text renderer should render text
   * based on the selection state of the items. This is necessary
   * for #ExoIconView, which doesn't draw any item state indicators
   * itself.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FOLLOW_STATE,
                                   g_param_spec_boolean ("follow-state",
                                                         "follow-state",
                                                         "follow-state",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * XfceTextRenderer:text:
   *
   * The text to render.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "text",
                                                        "text",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  /**
   * XfceTextRenderer:wrap-mode:
   *
   * Specifies how to break the string into multiple lines, if the cell renderer
   * does not have enough room to display the entire string. This property has
   * no effect unless the wrap-width property is set.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode",
                                                      "wrap-mode",
                                                      "wrap-mode",
                                                      PANGO_TYPE_WRAP_MODE,
                                                      PANGO_WRAP_CHAR,
                                                      EXO_PARAM_READWRITE));

  /**
   * XfceTextRenderer:wrap-width:
   *
   * Specifies the width at which the text is wrapped. The wrap-mode property can
   * be used to influence at what character positions the line breaks can be placed.
   * Setting wrap-width to -1 turns wrapping off.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_WRAP_WIDTH,
                                   g_param_spec_int ("wrap-width",
                                                     "wrap-width",
                                                     "wrap-width",
                                                     -1, G_MAXINT, -1,
                                                     EXO_PARAM_READWRITE));
}



static void
xfce_text_renderer_init (XfceTextRenderer *text_renderer)
{
  text_renderer->wrap_width = -1;
}



static void
xfce_text_renderer_finalize (GObject *object)
{
  XfceTextRenderer *text_renderer = XFCE_TEXT_RENDERER (object);

  /* release text (if not static) */
  if (!text_renderer->text_static)
    g_free (text_renderer->text);

  /* drop the cached widget */
  xfce_text_renderer_set_widget (text_renderer, NULL);

  (*G_OBJECT_CLASS (xfce_text_renderer_parent_class)->finalize) (object);
}



static void
xfce_text_renderer_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  XfceTextRenderer *text_renderer = XFCE_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_FOLLOW_PRELIT:
      g_value_set_boolean (value, text_renderer->follow_prelit);
      break;

    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, text_renderer->follow_state);
      break;

    case PROP_TEXT:
      g_value_set_string (value, text_renderer->text);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, text_renderer->wrap_mode);
      break;

    case PROP_WRAP_WIDTH:
      g_value_set_int (value, text_renderer->wrap_width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_text_renderer_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  XfceTextRenderer *text_renderer = XFCE_TEXT_RENDERER (object);
  const gchar      *sval;

  switch (prop_id)
    {
    case PROP_FOLLOW_PRELIT:
      text_renderer->follow_prelit = g_value_get_boolean (value);
      break;

    case PROP_FOLLOW_STATE:
      text_renderer->follow_state = g_value_get_boolean (value);
      break;

    case PROP_TEXT:
      /* release the previous text (if not static) */
      if (!text_renderer->text_static)
        g_free (text_renderer->text);
      sval = g_value_get_string (value);
      text_renderer->text_static = (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS);
      text_renderer->text = (sval == NULL) ? "" : (gchar *)sval;
      if (!text_renderer->text_static)
        text_renderer->text = g_strdup (text_renderer->text);
      break;

    case PROP_WRAP_MODE:
      text_renderer->wrap_mode = g_value_get_enum (value);
      break;

    case PROP_WRAP_WIDTH:
      /* be sure to reset fixed height if wrapping is requested */
      text_renderer->wrap_width = g_value_get_int (value);
      if (G_LIKELY (text_renderer->wrap_width >= 0))
        gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (text_renderer), -1, -1);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
xfce_text_renderer_get_size (GtkCellRenderer *renderer,
                             GtkWidget       *widget,
                             GdkRectangle    *cell_area,
                             gint            *x_offset,
                             gint            *y_offset,
                             gint            *width,
                             gint            *height)
{
  XfceTextRenderer *text_renderer = XFCE_TEXT_RENDERER (renderer);
  gint              text_length;
  gint              text_width;
  gint              text_height;

  /* setup the new widget */
  xfce_text_renderer_set_widget (text_renderer, widget);

  /* we can guess the dimensions if we don't wrap */
  if (text_renderer->wrap_width < 0)
    {
      /* determine the text_length in characters */
      text_length = g_utf8_strlen (text_renderer->text, -1);

      /* the approximation is usually 1-2 chars wrong, so wth */
      text_length += 2;

      /* calculate the appromixate text width/height */
      text_width = text_renderer->char_width * text_length;
      text_height = text_renderer->char_height;
    }
  else
    {
      /* calculate the real text dimension */
      pango_layout_set_width (text_renderer->layout, text_renderer->wrap_width * PANGO_SCALE);
      pango_layout_set_wrap (text_renderer->layout, text_renderer->wrap_mode);
      pango_layout_set_text (text_renderer->layout, text_renderer->text, -1);
      pango_layout_get_pixel_size (text_renderer->layout, &text_width, &text_height);
    }

  /* if we have to follow the state manually, we'll need
   * to reserve some space to render the indicator to.
   */
  if (text_renderer->follow_state)
    {
      text_width += 2 * text_renderer->focus_width;
      text_height += 2 * text_renderer->focus_width;
    }

  /* update width/height */
  if (G_LIKELY (width != NULL))
    *width = text_width + 2 * renderer->xpad;
  if (G_LIKELY (height != NULL))
    *height = text_height + 2 * renderer->ypad;

  /* update the x/y offsets */
  if (G_LIKELY (cell_area != NULL))
    {
      if (G_LIKELY (x_offset != NULL))
        {
          *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? (1.0 - renderer->xalign) : renderer->xalign)
                    * (cell_area->width - text_width - (2 * renderer->xpad));
          *x_offset = MAX (*x_offset, 0);
        }

      if (G_LIKELY (y_offset != NULL))
        {
          *y_offset = renderer->yalign * (cell_area->height - text_height - (2 * renderer->ypad));
          *y_offset = MAX (*y_offset, 0);
        }
    }
}



static PangoAttrList*
xfce_pango_attr_list_wrap (PangoAttribute *attribute, ...)
{
  PangoAttrList *attr_list;
  va_list        args;

  /* allocate a new attribute list */
  attr_list = pango_attr_list_new ();

  /* add all specified attributes */
  va_start (args, attribute);
  while (attribute != NULL)
    {
      attribute->start_index = 0;
      attribute->end_index = -1;
      pango_attr_list_insert (attr_list, attribute);
      attribute = va_arg (args, PangoAttribute *);
    }
  va_end (args);

  return attr_list;
}






/**
 * xfce_pango_attr_list_underline_single:
 *
 * Returns a #PangoAttrList for underlining text using a single line.
 * The returned list is owned by the callee and must not be freed
 * or modified by the caller.
 *
 * Return value: a #PangoAttrList for underlining text using a single line.
 **/
static PangoAttrList*
xfce_pango_attr_list_underline_single (void)
{
  static PangoAttrList *attr_list = NULL;
  if (G_UNLIKELY (attr_list == NULL))
    attr_list = xfce_pango_attr_list_wrap (pango_attr_underline_new (PANGO_UNDERLINE_SINGLE), NULL);
  return attr_list;
}



static void
xfce_text_renderer_render (GtkCellRenderer     *renderer,
                           GdkWindow           *window,
                           GtkWidget           *widget,
                           GdkRectangle        *background_area,
                           GdkRectangle        *cell_area,
                           GdkRectangle        *expose_area,
                           GtkCellRendererState flags)
{
  XfceTextRenderer *text_renderer = XFCE_TEXT_RENDERER (renderer);
  GtkStateType      state;
  cairo_t          *cr;
  gint              x0, x1, y0, y1;
  gint              text_width;
  gint              text_height;
  gint              x_offset;
  gint              y_offset;

  /* setup the new widget */
  xfce_text_renderer_set_widget (text_renderer, widget);

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
        state = GTK_STATE_SELECTED;
      else
        state = GTK_STATE_ACTIVE;
    }
  else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT
        && GTK_WIDGET_STATE (widget) == GTK_STATE_PRELIGHT)
    {
      state = GTK_STATE_PRELIGHT;
    }
  else
    {
      if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE)
        state = GTK_STATE_INSENSITIVE;
      else
        state = GTK_STATE_NORMAL;
    }

  /* check if we should follow the prelit state (used for single click support) */
  if (text_renderer->follow_prelit && (flags & GTK_CELL_RENDERER_PRELIT) != 0)
    pango_layout_set_attributes (text_renderer->layout, xfce_pango_attr_list_underline_single ());
  else
    pango_layout_set_attributes (text_renderer->layout, NULL);

  /* setup the wrapping */
  if (text_renderer->wrap_width < 0)
    {
      pango_layout_set_width (text_renderer->layout, -1);
      pango_layout_set_wrap (text_renderer->layout, PANGO_WRAP_CHAR);
    }
  else
    {
      pango_layout_set_width (text_renderer->layout, text_renderer->wrap_width * PANGO_SCALE);
      pango_layout_set_wrap (text_renderer->layout, text_renderer->wrap_mode);
    }

  pango_layout_set_text (text_renderer->layout, text_renderer->text, -1);

  /* calculate the real text dimension */
  pango_layout_get_pixel_size (text_renderer->layout, &text_width, &text_height);

  /* take into account the state indicator (required for calculation) */
  if (text_renderer->follow_state)
    {
      text_width += 2 * text_renderer->focus_width;
      text_height += 2 * text_renderer->focus_width;
    }

  /* calculate the real x-offset */
  x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? (1.0 - renderer->xalign) : renderer->xalign)
           * (cell_area->width - text_width - (2 * renderer->xpad));
  x_offset = MAX (x_offset, 0);

  /* calculate the real y-offset */
  y_offset = renderer->yalign * (cell_area->height - text_height - (2 * renderer->ypad));
  y_offset = MAX (y_offset, 0);

  /* render the state indicator */
  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED && text_renderer->follow_state)
    {
      /* calculate the text bounding box (including the focus padding/width) */
      x0 = cell_area->x + x_offset;
      y0 = cell_area->y + y_offset;
      x1 = x0 + text_width;
      y1 = y0 + text_height;

      /* Cairo produces nicer results than using a polygon
       * and so we use it directly if possible.
       */
      cr = gdk_cairo_create (window);
      cairo_move_to (cr, x0 + 5, y0);
      cairo_line_to (cr, x1 - 5, y0);
      cairo_curve_to (cr, x1 - 5, y0, x1, y0, x1, y0 + 5);
      cairo_line_to (cr, x1, y1 - 5);
      cairo_curve_to (cr, x1, y1 - 5, x1, y1, x1 - 5, y1);
      cairo_line_to (cr, x0 + 5, y1);
      cairo_curve_to (cr, x0 + 5, y1, x0, y1, x0, y1 - 5);
      cairo_line_to (cr, x0, y0 + 5);
      cairo_curve_to (cr, x0, y0 + 5, x0, y0, x0 + 5, y0);
      gdk_cairo_set_source_color (cr, &widget->style->base[state]);
      cairo_fill (cr);
      cairo_destroy (cr);
    }

  /* draw the focus indicator */
  if (text_renderer->follow_state && (flags & GTK_CELL_RENDERER_FOCUSED) != 0)
    {
      gtk_paint_focus (widget->style, window, GTK_WIDGET_STATE (widget), NULL, widget, "icon_view",
                       cell_area->x + x_offset, cell_area->y + y_offset, text_width, text_height);
    }

  /* get proper sizing for the layout drawing */
  if (text_renderer->follow_state)
    {
      text_width -= 2 * text_renderer->focus_width;
      text_height -= 2 * text_renderer->focus_width;
      x_offset += text_renderer->focus_width;
      y_offset += text_renderer->focus_width;
    }

  /* draw the text */
  gtk_paint_layout (widget->style, window, state, TRUE,
                    expose_area, widget, "cellrenderertext",
                    cell_area->x + x_offset + renderer->xpad,
                    cell_area->y + y_offset + renderer->ypad,
                    text_renderer->layout);
}



static void
xfce_text_renderer_invalidate (XfceTextRenderer *text_renderer)
{
  xfce_text_renderer_set_widget (text_renderer, NULL);
}



static void
xfce_text_renderer_set_widget (XfceTextRenderer *text_renderer,
                               GtkWidget          *widget)
{
  PangoFontMetrics *metrics;
  PangoContext     *context;
  gint              focus_padding;
  gint              focus_line_width;

  if (G_LIKELY (widget == text_renderer->widget))
    return;

  /* disconnect from the previously set widget */
  if (G_UNLIKELY (text_renderer->widget != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (text_renderer->widget), xfce_text_renderer_invalidate, text_renderer);
      g_object_unref (G_OBJECT (text_renderer->layout));
      g_object_unref (G_OBJECT (text_renderer->widget));
    }

  /* activate the new widget */
  text_renderer->widget = widget;

  /* connect to the new widget */
  if (G_LIKELY (widget != NULL))
    {
      /* take a reference on the widget */
      g_object_ref (G_OBJECT (widget));

      /* we need to recalculate the metrics when a new style (and thereby a new font) is set */
      g_signal_connect_swapped (G_OBJECT (text_renderer->widget), "destroy", G_CALLBACK (xfce_text_renderer_invalidate), text_renderer);
      g_signal_connect_swapped (G_OBJECT (text_renderer->widget), "style-set", G_CALLBACK (xfce_text_renderer_invalidate), text_renderer);

      /* allocate a new pango layout for this widget */
      context = gtk_widget_get_pango_context (widget);
      text_renderer->layout = pango_layout_new (context);

      /* disable automatic text direction, but use the direction specified by Gtk+ */
      pango_layout_set_auto_dir (text_renderer->layout, FALSE);

      /* we don't want to interpret line separators in file names */
      pango_layout_set_single_paragraph_mode (text_renderer->layout, TRUE);

      /* calculate the average character dimensions */
      metrics = pango_context_get_metrics (context, widget->style->font_desc, pango_context_get_language (context));
      text_renderer->char_width = PANGO_PIXELS (pango_font_metrics_get_approximate_char_width (metrics));
      text_renderer->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));
      pango_font_metrics_unref (metrics);

      /* tell the cell renderer about the fixed height if we're not wrapping text */
      if (G_LIKELY (text_renderer->wrap_width < 0))
        gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (text_renderer), -1, text_renderer->char_height);

      /* determine the focus-padding and focus-line-width style properties from the widget */
      gtk_widget_style_get (widget, "focus-padding", &focus_padding, "focus-line-width", &focus_line_width, NULL);
      text_renderer->focus_width = focus_padding + focus_line_width;
    }
  else
    {
      text_renderer->layout = NULL;
      text_renderer->char_width = 0;
      text_renderer->char_height = 0;
    }
}



/**
 * xfce_text_renderer_new:
 **/
GtkCellRenderer*
xfce_text_renderer_new (void)
{
  return g_object_new (XFCE_TYPE_TEXT_RENDERER, NULL);
}

