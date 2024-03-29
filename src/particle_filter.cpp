/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
    num_particles = 100;

    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<double> x_generator{x, std[0]};
    std::normal_distribution<double> y_generator{y, std[1]};
    std::normal_distribution<double> theta_generator{theta, std[2]};

    for (int i = 0; i < num_particles; i++) {
        weights.push_back(1.0);
        Particle p;
        p.id = i;
        p.x = x_generator(gen);
        p.y = y_generator(gen);
        p.theta = theta_generator(gen);
        p.weight = 1.0;
        particles.push_back(p);
    }

    is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::normal_distribution<double> x_generator{0, std_pos[0]};
    std::normal_distribution<double> y_generator{0, std_pos[1]};
    std::normal_distribution<double> theta_generator{0, std_pos[2]};
    double theta_offset = yaw_rate * delta_t;
    std::cout << "velocity = " << velocity << " yaw_rate = " << yaw_rate << std::endl;


    for (int i = 0; i < num_particles; i++) {
        // if yaw_rate equals zero, we need to use different equations to avoid division by zero
        if (fabs(yaw_rate) < 0.0001) {
            particles[i].x += velocity * cos(particles[i].theta) * delta_t;
            particles[i].y += velocity * sin(particles[i].theta) * delta_t;
        } else {
            particles[i].x +=
                    (velocity * (sin(particles[i].theta + theta_offset) - sin(particles[i].theta)) / yaw_rate)
                    + x_generator(gen);
            particles[i].y +=
                    (velocity * (cos(particles[i].theta) - cos(particles[i].theta + theta_offset)) / yaw_rate)
                    + y_generator(gen);
            particles[i].theta += theta_offset + theta_generator(gen);
        }
    }
}

Map::single_landmark_s ParticleFilter::findNearestLandmark(const LandmarkObs& map_observation, const Map &map_landmarks) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
    Map::single_landmark_s nearest = map_landmarks.landmark_list[0];
    double nearest_dist = dist(map_landmarks.landmark_list[0].x_f, map_landmarks.landmark_list[0].y_f, map_observation.x, map_observation.y);
    for (int i = 1; i < map_landmarks.landmark_list.size(); i++) {
        double distance = dist(map_landmarks.landmark_list[i].x_f, map_landmarks.landmark_list[i].y_f, map_observation.x,  map_observation.y);
        if (distance < nearest_dist) {
            nearest = map_landmarks.landmark_list[i];
            nearest_dist = distance;
        }
    }
    return nearest;
}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[],
                                   const vector<LandmarkObs> &observations,
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */

    for (int i = 0; i < num_particles; i++) {
        Particle& p = particles[i];
        vector<int> associations;
        vector<double> sense_x;
        vector<double> sense_y;
        double rotation_angle = p.theta;
        double weight = 1.0;
        for (int j = 0; j < observations.size(); j++) {
            double obs_map_x = p.x + observations[j].x * cos(rotation_angle) - observations[j].y * sin(rotation_angle);
            double obs_map_y = p.y + observations[j].x * sin(rotation_angle) + observations[j].y * cos(rotation_angle);
            LandmarkObs map_obs {-1, obs_map_x, obs_map_y};
            Map::single_landmark_s nearest_landmark = findNearestLandmark(map_obs, map_landmarks);
            sense_x.push_back(obs_map_x);
            sense_y.push_back(obs_map_y);
            associations.push_back(nearest_landmark.id_i);
            weight *= multivProb(std_landmark[0], std_landmark[1], obs_map_x, obs_map_y, nearest_landmark.x_f, nearest_landmark.y_f);
        }
        p.weight = weight;
        weights[i] = weight;
        SetAssociations(p, associations, sense_x, sense_y);
    }
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */
    std::random_device rd{};
    std::mt19937 gen{rd()};
    std::discrete_distribution<> d(weights.begin(), weights.end());
    std::vector<Particle> new_particles;
    new_particles.reserve(num_particles);
    for (int i = 0; i < num_particles; i++) {
        Particle p = particles[d(gen)];
        new_particles.push_back(p);
        weights[i] = p.weight;
    }
    particles = new_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}