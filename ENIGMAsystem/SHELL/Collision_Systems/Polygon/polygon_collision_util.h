/** Copyright (C) 2021 Nabeel Danish
***
*** This file is a part of the ENIGMA Development Environment.
***
*** ENIGMA is free software: you can redistribute it and/or modify it under the
*** terms of the GNU General Public License as published by the Free Software
*** Foundation, version 3 of the license or any later version.
***
*** This application and its source code is distributed AS-IS, WITHOUT ANY
*** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*** details.
***
*** You should have received a copy of the GNU General Public License along
*** with this code. If not, see <http://www.gnu.org/licenses/>
**/

////////////////////////////////////
// Collision Util Functions - Functions that are of utility to the main Collision Implementation functions
// . They provide mathematical computations.
////////////////////////////////////

#include "Universal_System/Object_Tiers/collisions_object.h"
#include "Universal_System/Instances/instance_system.h" 
#include "Universal_System/Instances/instance.h"
#include "Universal_System/math_consts.h"
#include "Universal_System/Resources/polygon.h"
#include "Universal_System/Resources/polygon_internal.h"
#include "../General/collisions_general.h"

#include <cmath>
#include <utility>

// DEFING GLOBAL CONSTANTS FOR CASES
enum collision_cases {POLYGON_VS_POLYGON, POLYGON_VS_BBOX, BBOX_VS_POLYGON, BBOX_VS_BBOX, POLYGON_VS_PREC, PREC_VS_POLYGON};

// -------------------------------------------------------------------------
// Function that returns the Minimum and Maximum
// Projection along an axis of a given normal
// set of points
//
// Args: 
//      vecs_box     -- a vector array of Vector2D points from a polygon normals
//      axis         -- a Vector2D specifying the axis
// Returns:
//      min_max_proj -- MinMaxProjection object that stores minmax information
// -------------------------------------------------------------------------
enigma::MinMaxProjection getMinMaxProjection(std::vector<glm::vec2>& vecs_box, glm::vec2 axis) {
    
    // Preparing
    double min_proj_box = glm::dot(vecs_box[0], axis);
    double max_proj_box = glm::dot(vecs_box[0], axis);
    int min_dot_box = 0;
    int max_dot_box = 0;

    // Iterating over the points
    for (int j = 1; j < vecs_box.size(); ++j) {
        double current_proj = glm::dot(vecs_box[j], axis);

        // Selecting Min
        if (min_proj_box > current_proj) {
            min_proj_box = current_proj;
            min_dot_box = j;
        }
        // Selecting Max
        if (current_proj > max_proj_box) {
            max_proj_box = current_proj;
            max_dot_box = j;
        }
    }

    // Filling return data
    enigma::MinMaxProjection minMaxProjection;
    minMaxProjection.min_projection = min_proj_box;
    minMaxProjection.max_projection = max_proj_box;
    minMaxProjection.max_index = max_dot_box;
    minMaxProjection.min_index = min_dot_box;

    return minMaxProjection;
}

// -------------------------------------------------------------------------
// Function that returns the projection points of an ellipse along the angle
// of the axis that is specified. This projection is basically the radius 
// of the ellipse that is skewed by that angle. It returns the two points 
// that intersect the ellipse and the line formed by the radius. These points 
// can then be used for constructing a polygon for minmax projection
//
// Args: 
//      angleOfAxis         -- a double that gives the angle (in radians) from the x-axis 
//                              (NOTE: calculation done on mathematical x-axis)
//      eps_x, eps_y        -- doubles that gives the center coordinate of the ellipse
//      rx, ry              -- doubles that gives the radii of the ellipse (in x and y axis
//                              respectively)
// Returns:
//      ellipse_points      -- vector of Vector2D points containing the line segment that is formed
//                              by the points on the projection
// -------------------------------------------------------------------------
std::vector<glm::vec2> getEllipseProjectionPoints(double angleOfAxis, double eps_x, double eps_y, double rx, double ry) {
    // Using the angle to calculate slew radii.
    // This assumes that the ellipse is perfectly horizontal with zero rotation
    double r_theta = (rx * ry) / ((pow(rx, 2) * pow(sin(angleOfAxis), 2)) + (pow(ry, 2) * pow(cos(angleOfAxis), 2)));

    // Using the offsets, the angle, and the new radius to find
    // the points for projections
    glm::vec2 min_proj_point(r_theta * cos(angleOfAxis) - eps_x, r_theta * sin(angleOfAxis) - eps_y);
    glm::vec2 max_proj_point(r_theta * cos(angleOfAxis) + eps_x, r_theta * sin(angleOfAxis) + eps_y);

    // Finalizing the polygon
    std::vector<glm::vec2> ellipse_points;
    ellipse_points.push_back(min_proj_point);
    ellipse_points.push_back(max_proj_point);

    return ellipse_points;
}


// -------------------------------------------------------------------------
// Function that returns whether or not two polygons of the two instances are
// colliding or not
//
// Args: 
//      points_poly1 -- points of first polygon
//      points_poly2 -- points of second polygon
// Returns:
//      bool         -- true if collision otherwise false
// -------------------------------------------------------------------------
bool get_polygon_polygon_collision(std::vector<glm::vec2> points_poly1, std::vector<glm::vec2> points_poly2) 
{
    bool isSeparated = false;

    // Preparing Normals
    std::vector<glm::vec2> normals_poly1 = enigma::computeNormals(points_poly1);
    std::vector<glm::vec2> normals_poly2 = enigma::computeNormals(points_poly2);

    // Using polygon1 normals to see if there is an axis 
    // on which collision is occuring
    for (int i = 0; i < normals_poly1.size(); ++i) 
    {
        enigma::MinMaxProjection result1, result2;

        // Get Min Max Projection of all the points on 
        // this normal vector
        result1 = getMinMaxProjection(points_poly1, normals_poly1[i]);
        result2 = getMinMaxProjection(points_poly2, normals_poly1[i]);

        // Checking Projections for Collision
        isSeparated = result1.max_projection < result2.min_projection || result2.max_projection < result1.min_projection;

        // Break if Separated
        if (isSeparated) 
        {
            break;
        }
    }

    // Using polygon2 normals to see if there is an 
    // axis on which collision is occuring
    if (!isSeparated) 
    {
        for (int i = 0; i < normals_poly2.size(); ++i) 
        {
            enigma::MinMaxProjection result1, result2;

            // Get Min Max Projection of all the points on 
            // this normal vector
            result1 = getMinMaxProjection(points_poly1, normals_poly2[i]);
            result2 = getMinMaxProjection(points_poly2, normals_poly2[i]);

            // Checking Projections for Collision
            isSeparated = result1.max_projection < result2.min_projection || result2.max_projection < result1.min_projection;

            // Break if Separated
            if (isSeparated) 
            {
                break;
            }
        }
    }

    // If there is a single axis where the separation is happening, than the polygons are 
    // not colliding
    if (!isSeparated) 
    {
        // printf("Collision Detected!\n");
        return true;
    } else 
    {
        // printf("No Collision Detected!\n");
        return false;
    }
}

// -------------------------------------------------------------------------
// Function that returns whether or not a polygon and an ellipse are
// colliding or not
//
// Args: 
//      points_poly -- points of the polygon
//      x2, y2      -- position of ellipse
//      rx, ry      -- radii of the ellipse (x-axis and y-axis)
// Returns:
//      bool        -- true if collision otherwise false
// -------------------------------------------------------------------------
bool get_polygon_ellipse_collision(std::vector<glm::vec2> &points_poly, double x2, double y2, double rx, double ry) 
{
    bool isSeparated = false;

    // Preparing Normals
    std::vector<glm::vec2> normals_poly = enigma::computeNormals(points_poly);

    // Using polygon1 normals
    for (int i = 0; i < normals_poly.size(); ++i) 
    {
        enigma::MinMaxProjection result1, result2;

        // First determine the points between which we wish to compute the angle on
        // These points correlate with the normals of the polygons that we computed 
        // above
        glm::vec2 point2 = points_poly[i];
        glm::vec2 point1;

        // The point is either the next one in the list, or the first one
        if (i == normals_poly.size() - 1) 
        {
            point1 = points_poly[0];
        } else 
        {
            point1 = points_poly[i + 1];
        }

        // Computing the angle and using that angle to get the projection of the ellipse
        double angleOfAxis = -enigma::angleBetweenVectors(point1, point2);
        std::vector<glm::vec2> ellipse_points = getEllipseProjectionPoints(angleOfAxis, x2, y2, rx, ry);

        // Get Min Max Projection of all the points on this normal vector
        result1 = getMinMaxProjection(points_poly, normals_poly[i]);
        result2 = getMinMaxProjection(ellipse_points, normals_poly[i]);

        // Checking Projections for Collision
        isSeparated = result1.max_projection < result2.min_projection || result2.max_projection < result1.min_projection;

        // Break if Separated
        if (isSeparated) 
        {
            break;
        }
    }

    if (!isSeparated) 
    {
        // printf("Collision Detected!\n");
        return true;
    } 
    else 
    {
        // printf("No Collision Detected!\n");
        return false;
    }
}

// -------------------------------------------------------------------------
// Function that makes a polygon from the given bbox dimensions
//
// Args: 
//      x1, y1          -- position of bbox
//      width           -- width of the bbox
//      height          -- height of the bbox
// Returns:
//      bbox_polygon    -- a polygon object made from the parameter values
// -------------------------------------------------------------------------
enigma::Polygon get_bbox_from_dimensions(double x1, double y1, int width, int height) 
{
    // Creating bbox vectors
    glm::vec2 top_left(x1, y1);
    glm::vec2 top_right(width, y1);
    glm::vec2 bottom_left(x1, height);
    glm::vec2 bottom_right(width, height);

    // Creating bbox polygons
    enigma::Polygon bbox_polygon;
    bbox_polygon.setWidth(width);
    bbox_polygon.setHeight(height);

    // Adding Polygon points
    bbox_polygon.addPoint(top_left);
    bbox_polygon.addPoint(top_right);
    bbox_polygon.addPoint(bottom_right);
    bbox_polygon.addPoint(bottom_left);

    return bbox_polygon;
}


// -------------------------------------------------------------------------
// Function that returns whether or not a polygon and a bbox are colliding
// or not
//
// Args: 
//      inst1       -- object collisions for the instance that has a polygon
//      inst2       -- object collisions for the instance that uses the bbox
// Returns:
//      inst2       -- if collision otherwise NULL
// -------------------------------------------------------------------------
enigma::object_collisions* const get_polygon_bbox_collision(enigma::object_collisions* const inst1, enigma::object_collisions* const inst2) 
{
    // polygon vs bbox
    const int collsprite_index2 = inst2->mask_index != -1 ? inst2->mask_index : inst2->sprite_index;
    enigma::Sprite& sprite2 = enigma::sprites.get(collsprite_index2);

    // Calculating points for bbox
    int w2 = sprite2.width;
    int h2 = sprite2.height;

    // Getting bbox points
    // TODO : optimize this please
    enigma::Polygon bbox_polygon = get_bbox_from_dimensions(0, 0, w2, h2);
    std::vector<glm::vec2> bbox_points = bbox_polygon.getPoints();
    enigma::offsetPoints(bbox_points, inst2->x, inst2->y);

    // Getting polygon points
    std::vector<glm::vec2> points_poly = enigma::polygons.get(inst1->polygon_index).getPoints();
    glm::vec2 pivot = enigma::polygons.get(inst1->polygon_index).computeCenter();
    enigma::transformPoints(points_poly, 
                                inst1->x, inst1->y, 
                                inst1->polygon_angle, pivot,
                                inst1->polygon_xscale, inst1->polygon_yscale);

    // Collision Detection
    return get_polygon_polygon_collision(points_poly, bbox_points)? inst2: NULL;
}

// -------------------------------------------------------------------------
// Function to detect collision between a point and a polygon
// Args:
//      inst    - instance that has a polygon to detect collision with
//      x1, y1  - position of the point
// Returns:
//              - true if collision otherwise false
// -------------------------------------------------------------------------
bool get_polygon_point_collision(enigma::object_collisions* inst, int x1, int y1)
{
    // Converting the point to a small box
    enigma::Polygon bbox_main = get_bbox_from_dimensions(0, 0, 0.2, 0.2);

    // Fetching points
    std::vector<glm::vec2> points_poly2 = enigma::polygons.get(inst->polygon_index).getPoints();
    std::vector<glm::vec2> bbox_points = bbox_main.getPoints();

    // Applying Transformations
    // Applying Transformations
    glm::vec2 pivot2 = enigma::polygons.get(inst->polygon_index).computeCenter();
    enigma::transformPoints(points_poly2, 
                                inst->x, inst->y, 
                                inst->polygon_angle, pivot2,
                                inst->polygon_xscale, inst->polygon_yscale);
    enigma::offsetPoints(bbox_points, x1, y1);

    // Collision detection
    return get_polygon_polygon_collision(bbox_points, points_poly2) != NULL;
}
