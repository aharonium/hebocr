
/***************************************************************************
 *            ho_objmap.c
 *
 *  Fri Aug 12 20:13:33 2005
 *  Copyright  2005-2007  Yaacov Zamir
 *  <kzamir@walla.co.il>
 ****************************************************************************/

/*  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifndef TRUE
#define TRUE -1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif

#include "ho_dimentions.h"

#include "ho_objmap.h"

ho_objlist *
ho_objlist_new ()
{
  ho_objlist *new_hocr_object_list;

  /* allocate memory for struct */
  new_hocr_object_list = (ho_objlist *) malloc (sizeof (ho_objlist));

  /* check for new memory */
  if (!new_hocr_object_list)
    return NULL;

  /* init struct values */
  new_hocr_object_list->size = 0;
  new_hocr_object_list->allocated_size = 1000;

  /* allocate memory for objects */
  new_hocr_object_list->objects =
    (ho_obj *) malloc (sizeof (ho_obj) * new_hocr_object_list->allocated_size);

  /* if no memeory for objects free the struct */
  if (!new_hocr_object_list->objects)
  {
    free (new_hocr_object_list);
    return NULL;
  }

  return new_hocr_object_list;
}

int
ho_objlist_free (ho_objlist * object_list)
{
  if (!object_list)
    return TRUE;

  /* free the text */
  if (object_list->objects)
    free (object_list->objects);

  /* free the struct */
  free (object_list);

  return FALSE;
}

int
ho_objlist_add (ho_objlist * object_list, double weight,
  int x, int y, int width, int height)
{
  ho_obj *new_object_list_objects;

  if ((object_list->size + 1) > 2000000000)
    return TRUE;

  /* check for allocated space and try to get more memory */
  if ((object_list->size + 1) >= object_list->allocated_size)
  {
    new_object_list_objects = (ho_obj *)
      realloc (object_list->objects, sizeof (ho_obj) *
      (object_list->allocated_size + 1000));

    /* got new memory */
    if (new_object_list_objects)
    {
      object_list->objects = new_object_list_objects;
      object_list->allocated_size += 1000;
    }
    /* did not get new memory */
    else
    {
      return TRUE;
    }
  }

  /* add the new object */
  (object_list->objects)[object_list->size].index = object_list->size;
  (object_list->objects)[object_list->size].reading_index = object_list->size;
  (object_list->objects)[object_list->size].weight = weight;
  (object_list->objects)[object_list->size].x = x;
  (object_list->objects)[object_list->size].y = y;
  (object_list->objects)[object_list->size].height = height;
  (object_list->objects)[object_list->size].width = width;
  (object_list->size)++;

  return FALSE;
}

int
ho_objlist_get_index (ho_objlist * object_list, int index)
{
  while ((object_list->objects)[index].index != index)
    index = (object_list->objects)[index].index;

  return index;
}

ho_obj *
ho_objlist_get_object (ho_objlist * object_list, int index)
{
  return &((object_list->objects)[index]);
}

int
ho_objlist_add_pixel (ho_objlist * object_list, int index, int x, int y)
{
  int x1 = (object_list->objects)[index].x;
  int x2 =
    (object_list->objects)[index].x + (object_list->objects)[index].width;
  int y1 = (object_list->objects)[index].y;
  int y2 =
    (object_list->objects)[index].y + (object_list->objects)[index].height;

  (object_list->objects)[index].weight++;
  if (x1 > x)
    x1 = x;
  if (y1 > y)
    y1 = y;
  if (x2 <= x)
    x2 = x + 1;
  if (y2 <= y)
    y2 = y + 1;

  (object_list->objects)[index].x = x1;
  (object_list->objects)[index].y = y1;
  (object_list->objects)[index].height = y2 - y1;
  (object_list->objects)[index].width = x2 - x1;

  return FALSE;
}

int
ho_objlist_link (ho_objlist * object_list, int index1, int index2)
{
  int index;
  int x11, x12, y11, y12;
  int x21, x22, y21, y22;

  /* just checking :-) */
  if (index1 == index2)
    return TRUE;

  /* always use the lower index */
  if (index2 < index1)
  {
    index = index1;
    index1 = index2;
    index2 = index;
  }

  /* set object1 extentions */
  x11 = (object_list->objects)[index1].x;
  x12 = (object_list->objects)[index1].x + (object_list->objects)[index1].width;
  y11 = (object_list->objects)[index1].y;
  y12 =
    (object_list->objects)[index1].y + (object_list->objects)[index1].height;

  /* set object2 extentions */
  x21 = (object_list->objects)[index2].x;
  x22 = (object_list->objects)[index2].x + (object_list->objects)[index2].width;
  y21 = (object_list->objects)[index2].y;
  y22 =
    (object_list->objects)[index2].y + (object_list->objects)[index2].height;

  /* adjust index2 index */
  (object_list->objects)[index2].index = index1;

  /* adjust index1 weight */
  (object_list->objects)[index1].weight +=
    (object_list->objects)[index2].weight;

  /* adjust index1 object cordiants */
  if (x21 < x11)
    x11 = x21;
  if (y21 < y11)
    y11 = y21;
  if (x22 > x12)
    x12 = x22;
  if (y22 > y12)
    y12 = y22;

  (object_list->objects)[index1].x = x11;
  (object_list->objects)[index1].y = y11;
  (object_list->objects)[index1].height = (y12 - y11);
  (object_list->objects)[index1].width = (x12 - x11);

  return FALSE;
}

int
ho_objlist_clean (ho_objlist * object_list, int **map)
{
  ho_objlist *temp_object_list;
  int new_index, i;

  /* allocate temporary list */
  temp_object_list = ho_objlist_new ();
  if (!temp_object_list)
    return TRUE;

  *map = (int *) calloc (object_list->size, sizeof (int));
  if (!(*map))
  {
    ho_objlist_free (temp_object_list);
    return TRUE;
  }

  /* copy only valid objects to temporary list */
  new_index = 0;
  for (i = 0; i < (object_list->size); i++)
  {
    /* this index does not point to anything */
    (*map)[i] = 0;
    if ((object_list->objects)[i].index == i)
    {
      ho_objlist_add (temp_object_list,
        ((object_list->objects)[i]).weight,
        ((object_list->objects)[i]).x,
        ((object_list->objects)[i]).y,
        ((object_list->objects)[i]).width, ((object_list->objects)[i]).height);
      /* this index point to a new index */
      (*map)[i] = new_index;
      /* increas index */
      new_index++;
    }
  }

  /* copy back objects from temporary list */
  object_list->size = 0;
  for (i = 0; i < (temp_object_list->size); i++)
  {
    ho_objlist_add (object_list,
      ((temp_object_list->objects)[i]).weight,
      ((temp_object_list->objects)[i]).x,
      ((temp_object_list->objects)[i]).y,
      ((temp_object_list->objects)[i]).width,
      ((temp_object_list->objects)[i]).height);
  }

  /* unref the temporary list */
  ho_objlist_free (temp_object_list);

  return FALSE;
}

int
ho_objlist_clean_by_reading_index (ho_objlist * object_list, int **map)
{
  ho_objlist *temp_object_list;
  int new_index, i, j;

  /* allocate temporary list */
  temp_object_list = ho_objlist_new ();
  if (!temp_object_list)
    return TRUE;

  *map = (int *) calloc (object_list->size, sizeof (int));
  if (!(*map))
  {
    ho_objlist_free (temp_object_list);
    return TRUE;
  }

  /* map by reading index */
  new_index = 0;
  for (i = 0; i < (object_list->size); i++)
  {
    for (j = 0; j < (object_list->size); j++)
      if ((object_list->objects)[j].reading_index == i)
      {
        /* this index is this reading order */
        (*map)[j] = i;

        ho_objlist_add (temp_object_list,
          ((object_list->objects)[j]).weight,
          ((object_list->objects)[j]).x,
          ((object_list->objects)[j]).y,
          ((object_list->objects)[j]).width,
          ((object_list->objects)[j]).height);
      }
  }

  /* copy back objects from temporary list */
  object_list->size = 0;
  for (i = 0; i < (temp_object_list->size); i++)
  {
    ho_objlist_add (object_list,
      ((temp_object_list->objects)[i]).weight,
      ((temp_object_list->objects)[i]).x,
      ((temp_object_list->objects)[i]).y,
      ((temp_object_list->objects)[i]).width,
      ((temp_object_list->objects)[i]).height);
  }

  /* unref the temporary list */
  ho_objlist_free (temp_object_list);

  return FALSE;
}

int
ho_objlist_print (ho_objlist * object_list)
{
  int i;

  printf ("list has %d objects\n---w\th\tw\n---\n", object_list->size);

  for (i = 1; i < (object_list->size); i++)
  {
    printf ("%0.0f\t%d\t%d\n", ((object_list->objects)[i]).weight,
      ((object_list->objects)[i]).height, ((object_list->objects)[i]).width);
  }

  return FALSE;
}

int
ho_objlist_statistics (ho_objlist * object_list,
  int min_height, int max_height,
  int min_width, int max_width,
  int *counter,
  double *weight_avg, double *weight_com,
  double *weight_min, double *weight_max,
  int *height_avg, int *height_com,
  int *height_min, int *height_max,
  int *width_avg, int *width_com, int *width_min, int *width_max)
{
  /* historam boxes are 0 .. 300 -> 0 .. 60 (5 values into 1 histogram box) for 
   * 1D attributes and 0 .. 1500 -> 0 .. 60 (25 into 1) for 2D attributes */
  double weight_histogram[60];
  int height_histogram[60];
  int width_histogram[60];
  double weight;
  double weight_sum;
  int height;
  double height_sum;
  int width;
  double width_sum;
  int i, histogram_index;
  int histogram_index_weight = 0;
  int histogram_index_height = 0;
  int histogram_index_width = 0;

  /* clean histograms */
  for (i = 0; i < 60; i++)
  {
    weight_histogram[i] = 0.0;
    height_histogram[i] = 0;
    width_histogram[i] = 0;
  }

  /* clean initial sum, min and max values */
  weight = ((object_list->objects)[0]).weight;
  width = ((object_list->objects)[0]).width;
  height = ((object_list->objects)[0]).height;

  *weight_min = *weight_max = weight_sum = weight;
  *width_min = *width_max = width_sum = width;
  *height_min = *height_max = height_sum = height;

  (*counter) = 0;

  for (i = 1; i < (object_list->size); i++)
  {
    /* get values */
    weight = ((object_list->objects)[i]).weight;
    width = ((object_list->objects)[i]).width;
    height = ((object_list->objects)[i]).height;

    if (width > min_width && width < max_width && height > min_height
      && height < max_height)
    {

      (*counter)++;

      /* set min max and sum */
      weight_sum += weight;
      if ((*weight_min) > weight)
        *weight_min = weight;
      if ((*weight_max) < weight)
        *weight_max = weight;

      width_sum += width;
      if ((*width_min) > width)
        *width_min = width;
      if ((*width_max) < width)
        *width_max = width;

      height_sum += height;
      if ((*height_min) > height)
        *height_min = height;
      if ((*height_max) < height)
        *height_max = height;

      /* set histograms */
      histogram_index = weight / 25;
      if (histogram_index < 60)
        (weight_histogram[histogram_index])++;
      histogram_index = height / 5;
      if (histogram_index < 60)
        (height_histogram[histogram_index])++;
      histogram_index = width / 5;
      if (histogram_index < 60)
        (width_histogram[histogram_index])++;
      
    }
  }

  /* set average values (default to 0 when no object passed the filter) */
  *weight_avg = 0.0;
  *height_avg = 0;
  *width_avg = 0;
  if (*counter)
  {
    *weight_avg = weight_sum / (*counter);
    *height_avg = height_sum / (*counter);
    *width_avg = width_sum / (*counter);
  }

  /* set common values */
  for (i = 0; i < 60; i++)
  {
    if (weight_histogram[histogram_index_weight] < weight_histogram[i])
      histogram_index_weight = i;
    if (height_histogram[histogram_index_height] < height_histogram[i])
      histogram_index_height = i;
    if (width_histogram[histogram_index_width] < width_histogram[i])
      histogram_index_width = i;
  }

  /* set common values */
  *weight_com = (double) histogram_index_weight *25.0 + 12;

  *height_com = histogram_index_height * 5 + 2;
  *width_com = histogram_index_width * 5 + 2;

  /* if fond some common height & width values fine-tune the height & width */
  if (height_histogram[histogram_index_height] > 0)
  {
    /* empty the height & width histogram */
    for (i = 0; i < 60; i++)
    {
      height_histogram[i] = 0;
      width_histogram[i] = 0;
    }

    for (i = 1; i < (object_list->size); i++)
    {
      /* get values */
      height = ((object_list->objects)[i]).height;
      width = ((object_list->objects)[i]).width;

      if (width > min_width && width < max_width && height > min_height
      && height < max_height)
      {

        /* check for common only height */
        if (height >= (*height_com) - 2 && height <= (*height_com) + 2)
        {
          (height_histogram[height - (*height_com) + 2])++;
        }

        /* check for common only width */
        if (width >= (*width_com) - 2 && width <= (*width_com) + 2)
        {
          (width_histogram[width - (*width_com) + 2])++;
        }
      }
    }

    /* re-set common values */
    histogram_index_height = 0;
    histogram_index_width = 0;
    for (i = 0; i < 5; i++)
    {
      if (height_histogram[histogram_index_height] < height_histogram[i])
        histogram_index_height = i;
      if (width_histogram[histogram_index_width] < width_histogram[i])
        histogram_index_width = i;
    }

    /* re-set height common values */
    *height_com = histogram_index_height + (*height_com) - 2;
    *width_com = histogram_index_width + (*width_com) - 2;
  }

  return FALSE;
}

ho_objmap *
ho_objmap_new (const int width, const int height)
{
  ho_objmap *m_new = NULL;

  /* 
   * allocate memory for pixbuf 
   */
  m_new = (ho_objmap *) malloc (sizeof (ho_objmap));
  if (!m_new)
  {
    return NULL;
  }

  /* read header */
  m_new->x = 0;
  m_new->y = 0;
  m_new->width = width;
  m_new->height = height;

  /* allocate memory for data (and set to zero) */
  m_new->map = (int *) calloc (m_new->height * m_new->width, sizeof (int));
  if (!(m_new->map))
  {
    free (m_new);
    return NULL;
  }

  /* allocate initial object list */
  m_new->obj_list = ho_objlist_new ();
  if (!(m_new->obj_list))
  {
    free (m_new->map);
    free (m_new);
    return NULL;
  }

  return m_new;
}

int
ho_objmap_free (ho_objmap * m)
{
  if (!m)
    return TRUE;

  ho_objlist_free (m->obj_list);
  if (m->map)
    free (m->map);
  free (m);

  return FALSE;
}

int
ho_objmap_clean (ho_objmap * m)
{
  int x, y, k;
  int index;
  int *map = NULL;

  /* relax the object matrix */
  for (x = 0; x < m->width; x++)
    for (y = 0; y < m->height; y++)
    {

      k = ho_objmap_get (m, x, y);
      if (k)
      {
        index = ho_objlist_get_index ((m->obj_list), k - 1);
        ho_objmap_set (m, x, y, index + 1);
      }
    }

  /* clean the object list */
  ho_objlist_clean ((m->obj_list), &map);

  /* re relax the object matrix */
  for (x = 0; x < m->width; x++)
    for (y = 0; y < m->height; y++)
    {
      k = ho_objmap_get (m, x, y);
      if (k)
      {
        index = map[k - 1];
        ho_objmap_set (m, x, y, index + 1);
      }
    }

  /* free all temporary memory */
  if (map)
    free (map);

  return FALSE;
}

int
ho_objmap_sort_by_reading_index (ho_objmap * m, const unsigned char col,
  const unsigned char dir_ltr)
{
  int x, y, k;
  int index;
  int *map = NULL;

  /* make sure reading order is set */
  ho_objmap_update_reading_index (m, col, dir_ltr);

  /* clean the object list */
  ho_objlist_clean_by_reading_index ((m->obj_list), &map);

  /* re relax the object matrix */
  for (x = 0; x < m->width; x++)
    for (y = 0; y < m->height; y++)
    {
      k = ho_objmap_get (m, x, y);
      if (k)
      {
        index = map[k - 1];
        ho_objmap_set (m, x, y, index + 1);
      }
    }

  /* free all temporary memory */
  if (map)
    free (map);

  return FALSE;
}

ho_objmap *
ho_objmap_new_from_bitmap (const ho_bitmap * bit_in)
{
  ho_objmap *m_new = NULL;

  int x, y, k;
  double sum;
  double neigbors[4];
  int min;
  int indexes[4];

  /* allocate memory */
  m_new = ho_objmap_new (bit_in->width, bit_in->height);
  if (!m_new)
    return NULL;
  m_new->x = bit_in->x;
  m_new->y = bit_in->y;

  /* link all concted black pixels */
  for (x = 1; x < bit_in->width; x++)
    for (y = 1; y < (bit_in->height - 1); y++)
    {
      /* is pixel black ? */
      if (ho_bitmap_get (bit_in, x, y))
      {
        /* get neigbors */
        neigbors[0] = ho_objmap_get (m_new, x - 1, y - 1);
        neigbors[1] = ho_objmap_get (m_new, x - 1, y - 0);
        neigbors[2] = ho_objmap_get (m_new, x - 1, y + 1);
        neigbors[3] = ho_objmap_get (m_new, x - 0, y - 1);

        sum = neigbors[0] + neigbors[1] + neigbors[2] + neigbors[3];

        /* if no neigbor is black */
        if (sum == 0)
        {
          if (ho_objlist_add ((m_new->obj_list), 1, x, y, 1, 1))
          {
            /* can't add to objects :-( */
            ho_objmap_free (m_new);
            return NULL;
          }

          /* in the matrix we insert index+1 */
          ho_objmap_set (m_new, x, y, (m_new->obj_list)->size);
        }
        /* at least one neigbor is black */
        else
        {
          /* find the index of the minimal frind */
          min = 0;
          for (k = 0; k < 4; k++)
          {
            if (neigbors[k])
            {
              indexes[k] =
                ho_objlist_get_index ((m_new->obj_list), neigbors[k] - 1);
              if (!min || (indexes[k] < (min - 1)))
                /* indexes goes 0.. and min goes 1.. */
                min = (indexes[k] + 1);
            }
            else
              indexes[k] = 0;
          }
          /* min 1.. -> 0.. */
          min--;

          /* set the current pixel in the objects matrix to that index */
          ho_objmap_set (m_new, x, y, min + 1);
          ho_objlist_add_pixel ((m_new->obj_list), min, x, y);

          /* link all object to that index */
          for (k = 0; k < 4; k++)
          {
            if (indexes[k])
              ho_objlist_link ((m_new->obj_list), min, indexes[k]);
          }
        }
      }
    }

  /* relax and clean the objmap */
  ho_objmap_clean (m_new);

  return m_new;
}

int
ho_objmap_font_metrix (const ho_objmap * m, const int min_height,
  const int max_height,
  const int min_width,
  const int max_width, int *height, int *width, unsigned char *nikud)
{
  int counter;
  double weight_avg;
  double weight_com;
  double weight_min;
  double weight_max;
  int height_avg;
  int height_com;
  int height_min;
  int height_max;
  int width_avg;
  int width_com;
  int width_min;
  int width_max;

  *nikud = FALSE;

  /* get stats */
  ho_objlist_statistics (m->obj_list,
    min_height, max_height,
    min_width, max_width,
    &counter,
    &weight_avg, &weight_com,
    &weight_min, &weight_max,
    &height_avg, &height_com,
    &height_min, &height_max, &width_avg, &width_com, &width_min, &width_max);

  /* does it look like nikud ? */
  if (height_avg < (2 * height_com / 3))
  {
    *nikud = TRUE;
  }

  /* did we find the nikud ? */
  if (height_com < (2 * height_avg / 3))
  {
    *nikud = TRUE;

    /* re-get stats, now limit minial size */
    ho_objlist_statistics (m->obj_list, 3 * height_avg / 2, 300,
      3 * width_avg / 2, 300, &counter,
      &weight_avg, &weight_com, &weight_min,
      &weight_max, &height_avg, &height_com,
      &height_min, &height_max, &width_avg, &width_com, &width_min, &width_max);
  }

  /* is width logical ? */
  if (height_com > 3 * width_com / 2)
    width_com = 2 * height_com / 3;

  *height = height_com;
  *width = width_com;

  return FALSE;
}

ho_bitmap *
ho_objmap_to_bitmap (const ho_objmap * obj_in)
{
  int x, y;
  ho_bitmap *pix = NULL;
  int index = 0;

  /* allocate memory */
  pix = ho_bitmap_new (obj_in->width, obj_in->height);
  if (!pix)
    return NULL;

  /* copy pixels from gsl mtirxes */
  for (x = 0; x < obj_in->width; x++)
    for (y = 0; y < obj_in->height; y++)
    {
      index = ho_objmap_get (obj_in, x, y);

      if (index)
      {
        /* set autput colors */
        ho_bitmap_set (pix, x, y);
      }
    }

  return pix;
}

ho_bitmap *
ho_objmap_to_bitmap_by_size (const ho_objmap * m,
  int min_height, int max_height, int min_width, int max_width)
{
  int x, y;
  int index;
  int height;
  int width;
  ho_bitmap *m_out;

  /* allocate memory */
  m_out = ho_bitmap_new (m->width, m->height);
  if (!m_out)
    return NULL;

  m_out->x = m->x;
  m_out->y = m->y;

  for (x = 0; x < m->width; x++)
    for (y = 0; y < m->height; y++)
    {
      index = ho_objmap_get (m, x, y);

      if (index)
      {
        /* on objmap objects are 1.. and on objlist 0.. */
        index--;

        width = (((m->obj_list)->objects)[index]).width;
        height = (((m->obj_list)->objects)[index]).height;

        if (width < min_width || width > max_width || height < min_height
          || height > max_height)
          continue;

        ho_bitmap_set (m_out, x, y);
      }
    }

  return m_out;
}

ho_bitmap *
ho_objmap_to_bitmap_by_index (const ho_objmap * m, const int index)
{
  int x, y;
  ho_bitmap *m_out;
  int current_index;
  int height;
  int width;

  /* is this object realy an object ? */
  if (index >= (m->obj_list)->size)
    return NULL;

  width = (((m->obj_list)->objects)[index]).width;
  height = (((m->obj_list)->objects)[index]).height;

  /* is this object realy a big object ? */
  if (width < 2 || height < 2)
    return NULL;

  /* allocate memory */
  m_out = ho_bitmap_new (m->width, m->height);
  if (!m_out)
    return NULL;

  m_out->x = m->x;
  m_out->y = m->y;

  for (x = 0; x < m->width; x++)
    for (y = 0; y < m->height; y++)
    {
      current_index = ho_objmap_get (m, x, y);

      if ((current_index - 1) == index)
      {
        ho_bitmap_set (m_out, x, y);
      }
    }

  return m_out;
}

ho_bitmap *
ho_objmap_to_bitmap_by_index_window (const ho_objmap * m,
  const int index, const int frame)
{
  int x, y;
  int x1, y1;
  ho_bitmap *m_out;
  int current_index;
  int height;
  int width;

  /* is this object realy an object ? */
  if (index >= (m->obj_list)->size)
    return NULL;

  width = (((m->obj_list)->objects)[index]).width;
  height = (((m->obj_list)->objects)[index]).height;

  /* is this object realy a big object ? */
  if (width < 2 || height < 2)
    return NULL;

  /* allocate memory */
  m_out = ho_bitmap_new (width + 2 * frame, height + 2 * frame);
  if (!m_out)
    return NULL;

  x1 = (((m->obj_list)->objects)[index]).x;
  y1 = (((m->obj_list)->objects)[index]).y;

  m_out->x = m->x + x1 - frame;
  m_out->y = m->y + y1 - frame;

  for (x = 0; x < width; x++)
    for (y = 0; y < height; y++)
    {
      current_index = ho_objmap_get (m, x + x1, y + y1);

      if ((current_index - 1) == index)
      {
        ho_bitmap_set (m_out, x + frame, y + frame);
      }
    }

  return m_out;
}

int
ho_objmap_update_reading_index_line (ho_objmap * m,
  const unsigned char n_columns, const unsigned char dir_ltr)
{
  int index;
  int reading_index;
  int x;

  /* if n_columns != 253 then this is not one line sorting */
  if (n_columns != 253)
    return TRUE;

  reading_index = 0;

  if (dir_ltr)
  {
    for (x = 0; x < m->width; x++)
    {
      for (index = 0; index < ho_objmap_get_size (m); index++)
      {
        if ((ho_objmap_get_object (m, index).x +
            ho_objmap_get_object (m, index).width - 1) == x)
        {
          ho_objmap_get_object (m, index).reading_index = reading_index;
          reading_index++;
        }
      }
    }
  }
  else
  {
    for (x = m->width; x >= 0; x--)
    {
      for (index = 0; index < ho_objmap_get_size (m); index++)
      {
        if ((ho_objmap_get_object (m, index).x +
            ho_objmap_get_object (m, index).width - 1) == x)
        {
          ho_objmap_get_object (m, index).reading_index = reading_index;
          reading_index++;
        }
      }
    }
  }

  return FALSE;
}

int
ho_objmap_update_reading_index_column (ho_objmap * m,
  const unsigned char n_columns, const unsigned char dir_ltr)
{
  int index;
  int reading_index;
  int y;

  /* if n_columns != 1 then this is not one column sorting */
  if (n_columns != 1)
    return TRUE;

  reading_index = 0;

  for (y = 0; y < m->height; y++)
  {
    for (index = 0; index < ho_objmap_get_size (m); index++)
    {
      if ((ho_objmap_get_object (m, index).y) == y)
      {
        ho_objmap_get_object (m, index).reading_index = reading_index;
        reading_index++;
      }
    }
  }

  return FALSE;
}

int
ho_objmap_update_reading_index_multi_columns (ho_objmap * m,
  const unsigned char n_columns, const unsigned char dir_ltr)
{
  int index;
  int k, column;
  int reading_index;
  int y;
  int edge;
  int size;
  int x_start, x_end;
  int column_start_list_size;
  int *column_start_list = NULL;
  unsigned char *assigned = NULL;
  unsigned char *band = NULL;
  int *col = NULL;
  int *col_min = NULL;
  int *col_max = NULL;
  int *order = NULL;
  int n;
  ho_bitmap *m_bitmap = NULL;

  /*
   * 2..6 columns, or 252: find the columns from the blocks. Other values
   * mean "leave the order alone": letter slicing (ho_segment.c) relies on
   * 255 doing nothing.
   */
  if (n_columns < 2 || (n_columns > 6 && n_columns != 252))
    return TRUE;

  size = ho_objmap_get_size (m);
  if (size < 2)
    return ho_objmap_update_reading_index_column (m, 1, dir_ltr);

  /*
   * Get the columns' start list (rightmost column first). Look for the
   * gutters using only blocks narrower than about half the text area:
   * a title or full-width paragraph covers the gutter and would hide the
   * columns. If there are no narrow blocks, use them all.
   */
  {
    int span_min = m->width, span_max = 0;

    for (index = 0; index < size; index++)
    {
      if (ho_objmap_get_object (m, index).x < span_min)
        span_min = ho_objmap_get_object (m, index).x;
      if (ho_objmap_get_object (m, index).x +
        ho_objmap_get_object (m, index).width > span_max)
        span_max = ho_objmap_get_object (m, index).x +
          ho_objmap_get_object (m, index).width;
    }
    m_bitmap = ho_objmap_to_bitmap_by_size (m, 0, m->height, 0,
      (span_max - span_min) * 55 / 100);
    if (m_bitmap)
    {
      column_start_list_size =
        ho_dimentions_get_columns_with_x_start (m_bitmap, &column_start_list);
      ho_bitmap_free (m_bitmap);
    }
    if (!column_start_list)
    {
      m_bitmap = ho_objmap_to_bitmap (m);
      if (!m_bitmap)
        return TRUE;
      column_start_list_size =
        ho_dimentions_get_columns_with_x_start (m_bitmap, &column_start_list);
      ho_bitmap_free (m_bitmap);
    }
  }

  /* column detection failed: fall back to single-column reading order */
  if (!column_start_list)
    return ho_objmap_update_reading_index_column (m, 1, dir_ltr);

  assigned = calloc (size, sizeof (unsigned char));
  if (!assigned)
  {
    free (column_start_list);
    return TRUE;
  }

  /*
   * Assign each block to the column that contains the edge where its
   * lines start: the right edge for right-to-left text, the left edge for
   * left-to-right text.
   */
  col = malloc (size * sizeof (int));
  band = calloc (size, sizeof (unsigned char));
  col_min = malloc (column_start_list_size * sizeof (int));
  col_max = malloc (column_start_list_size * sizeof (int));
  if (!col || !band || !col_min || !col_max)
  {
    free (col);
    free (band);
    free (col_min);
    free (col_max);
    free (assigned);
    free (column_start_list);
    return TRUE;
  }
  /* blocks starting on the same row are taken in the text's direction */
  order = malloc (size * sizeof (int));
  if (!order)
  {
    free (col);
    free (band);
    free (col_min);
    free (col_max);
    free (assigned);
    free (column_start_list);
    return TRUE;
  }
  for (index = 0; index < size; index++)
    order[index] = index;
  for (index = 1; index < size; index++)
  {
    int key = order[index], n = index - 1;
    int key_x = ho_objmap_get_object (m, key).x +
      (dir_ltr ? 0 : ho_objmap_get_object (m, key).width);
    while (n >= 0)
    {
      int nx = ho_objmap_get_object (m, order[n]).x +
        (dir_ltr ? 0 : ho_objmap_get_object (m, order[n]).width);
      if (dir_ltr ? nx <= key_x : nx >= key_x)
        break;
      order[n + 1] = order[n];
      n--;
    }
    order[n + 1] = key;
  }

  for (index = 0; index < size; index++)
  {
    col[index] = -1;
    edge = dir_ltr ? ho_objmap_get_object (m, index).x
      : ho_objmap_get_object (m, index).x +
      ho_objmap_get_object (m, index).width - 1;
    for (column = 0; column < column_start_list_size; column++)
    {
      x_start = column_start_list[column];
      x_end = (column == 0) ? m->width : column_start_list[column - 1];
      if (edge >= x_start && edge < x_end)
        col[index] = column;
    }
  }

  /*
   * Measure each column's width from its "core" blocks: those that sit
   * side by side with a block of another column. If a column has none,
   * use all of its blocks.
   */
  for (column = 0; column < column_start_list_size; column++)
  {
    int pass;

    col_min[column] = m->width;
    col_max[column] = -1;
    for (pass = 0; pass < 2 && col_max[column] < 0; pass++)
      for (index = 0; index < size; index++)
      {
        int other, core = 0;
        ho_obj *a = &ho_objmap_get_object (m, index);

        if (col[index] != column)
          continue;
        for (other = 0; pass == 0 && other < size && !core; other++)
        {
          ho_obj *b = &ho_objmap_get_object (m, other);
          if (col[other] >= 0 && col[other] != column
            && a->y < b->y + b->height && b->y < a->y + a->height)
            core = 1;
        }
        if (pass == 1 || core)
        {
          if (a->x < col_min[column])
            col_min[column] = a->x;
          if (a->x + a->width > col_max[column])
            col_max[column] = a->x + a->width;
        }
      }
  }

  /*
   * A block that belongs to no column, or does not fit within its
   * column's width (a title, a full-width paragraph), is a band across
   * the page.
   */
  for (index = 0; index < size; index++)
  {
    ho_obj *a = &ho_objmap_get_object (m, index);
    int tol;

    if (col[index] < 0 || col_max[col[index]] < 0)
    {
      band[index] = 1;
      continue;
    }
    tol = (col_max[col[index]] - col_min[col[index]]) / 20 + 2;
    if (a->x < col_min[col[index]] - tol
      || a->x + a->width > col_max[col[index]] + tol)
      band[index] = 1;
  }

  /*
   * Read the columns in the text's direction (right to left for Hebrew),
   * each top to bottom, down to the next band; then the band; and so on.
   */
  reading_index = 0;
  for (y = 0; y <= m->height; y++)
  {
    int band_here = -1;

    if (y < m->height)
      for (n = 0; n < size; n++)
        if (band[index = order[n]] && !assigned[index]
          && ho_objmap_get_object (m, index).y == y)
        {
          band_here = index;
          break;
        }
    if (band_here < 0 && y < m->height)
      continue;

    /* the columns above this band (or the rest of the page) */
    for (k = 0; k < column_start_list_size; k++)
    {
      int yy;

      column = dir_ltr ? column_start_list_size - 1 - k : k;
      for (yy = 0; yy < y; yy++)
        for (n = 0; n < size; n++)
          if (!assigned[index = order[n]] && !band[index] && col[index] == column
            && ho_objmap_get_object (m, index).y == yy)
          {
            ho_objmap_get_object (m, index).reading_index = reading_index;
            reading_index++;
            assigned[index] = 1;
          }
    }

    /* then every band starting on this row */
    if (y < m->height)
      for (n = 0; n < size; n++)
        if (band[index = order[n]] && !assigned[index]
          && ho_objmap_get_object (m, index).y == y)
        {
          ho_objmap_get_object (m, index).reading_index = reading_index;
          reading_index++;
          assigned[index] = 1;
        }
  }

  free (col);
  free (band);
  free (col_min);
  free (col_max);

  /* blocks outside every detected column follow, top to bottom, so every
   * block gets a unique reading index and no text is lost */
  for (y = 0; y < m->height; y++)
    for (n = 0; n < size; n++)
      if (!assigned[index = order[n]] && ho_objmap_get_object (m, index).y == y)
      {
        ho_objmap_get_object (m, index).reading_index = reading_index;
        reading_index++;
        assigned[index] = 1;
      }

  free (order);
  free (assigned);
  free (column_start_list);

  return FALSE;
}

int
ho_objmap_update_reading_index (ho_objmap * m,
  const unsigned char n_columns, const unsigned char dir_ltr)
{
  /* if n_columns == 1 then this is a one column sorting */
  if (n_columns == 1)
  {
    return ho_objmap_update_reading_index_column (m, n_columns, dir_ltr);
  }

  /* if n_columns == 253 then this is a one line sorting */
  if (n_columns == 253)
  {
    return ho_objmap_update_reading_index_line (m, n_columns, dir_ltr);
  }

  /* must be a multi column page */
  return ho_objmap_update_reading_index_multi_columns (m, n_columns, dir_ltr);

  return FALSE;
}

ho_obj *
ho_objmap_get_object_by_index (ho_objmap * m, int index)
{
  return &((m->obj_list->objects)[index]);
}
