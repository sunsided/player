
/**************************************************************************
 * Desc: Particle filter; drawing routines
 * Author: Andrew Howard
 * Date: 10 Dec 2002
 * CVS: $Id$
 *************************************************************************/

#include <assert.h>
#include <math.h>
#include <stdlib.h>

#include "rtk.h"

#include "pf.h"
#include "pf_pdf.h"
#include "pf_kdtree.h"


// Recursively draw the kd tree
void pf_draw_tree_node(pf_t *pf, rtk_fig_t *fig, pf_kdtree_t *tree, pf_kdtree_node_t *node);

// Draw the statistics
void pf_draw_statistics(pf_t *pf, rtk_fig_t *fig);


// Draw the sample set
void pf_draw_samples(pf_t *pf, rtk_fig_t *fig)
{
  int i;
  double px, py;
  pf_sample_set_t *set;
  pf_sample_t *sample;
  pf_kdtree_t *tree;

  set = pf->sets + pf->current_set;

  tree = pf_kdtree_alloc();
    
  for (i = 0; i < set->sample_count; i++)
  {
    sample = set->samples + i;

    //pf_kdtree_insert(tree, sample->pose);
    
    px = sample->pose.v[0];
    py = sample->pose.v[1];

    //printf("%f %f\n", px, py);
    
    //rtk_fig_point(fig, px, py);
    rtk_fig_rectangle(fig, px, py, 0, 0.1, 0.1, 0);
  }

  // Draw the tree
  //pf_draw_tree_node(pf, fig, tree, tree->root);
  
  pf_kdtree_free(tree);

  // Draw the statistics
  //pf_draw_statistics(pf, fig);
  
  return;
}


// Recursively draw the kd tree
void pf_draw_tree_node(pf_t *pf, rtk_fig_t *fig, pf_kdtree_t *tree, pf_kdtree_node_t *node)
{
  double px, py, sx, sy;
  
  if (node == NULL)
    return;
  
  sx = 0.50;
  sy = 0.50;

  px = sx * (node->key[0] + 0.5);
  py = sy * (node->key[1] + 0.5);
  
  rtk_fig_rectangle(fig, px, py, 0, sx, sy, 0);
  
  pf_draw_tree_node(pf, fig, tree, node->children[0]);
  pf_draw_tree_node(pf, fig, tree, node->children[1]);
  
  return;
}


// Draw the statistics
void pf_draw_statistics(pf_t *pf, rtk_fig_t *fig)
{
  int i;
  pf_vector_t mean;
  pf_matrix_t cov;
  pf_matrix_t r, d;
  double o, d1, d2;

  // Compute the distributions statistics
  pf_statistics(pf, &mean, &cov);

  // Take the linear components only
  for (i = 0; i < 3; i++)
  {
    cov.m[i][2] = 0;
    cov.m[2][i] = 0;
  }
  pf_matrix_svd(&r, &d, cov);
  
  // Compute the orientation of the error ellipse
  o = atan2(-r.m[0][1], r.m[0][0]);
  d1 = 6 * sqrt(d.m[0][0]);
  d2 = 6 * sqrt(d.m[1][1]);
  
  // Draw the error ellipse
  rtk_fig_ellipse(fig, mean.v[0], mean.v[1], o, d1, d2, 0);
  
  return;
}
