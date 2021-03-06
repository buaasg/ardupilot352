// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-
/*
 *  logic for dealing with the current command in the mission and home location
 */

#include "Plane.h"

/*
 *  set_next_WP - sets the target location the vehicle should fly to
 */
void Plane::set_next_WP(const struct Location &loc)
{
    if (auto_state.next_wp_no_crosstrack) {
        // we should not try to cross-track for this waypoint
        prev_WP_loc = current_loc;
        // use cross-track for the next waypoint
        auto_state.next_wp_no_crosstrack = false;
        auto_state.no_crosstrack = true;
    } else {
        // copy the current WP into the OldWP slot
        prev_WP_loc = next_WP_loc;
        auto_state.no_crosstrack = false;
    }

    // Load the next_WP slot
    // ---------------------
    next_WP_loc = loc;

    // if lat and lon is zero, then use current lat/lon
    // this allows a mission to contain a "loiter on the spot"
    // command
    if (next_WP_loc.lat == 0 && next_WP_loc.lng == 0) {
        next_WP_loc.lat = current_loc.lat;
        next_WP_loc.lng = current_loc.lng;
        // additionally treat zero altitude as current altitude
        if (next_WP_loc.alt == 0) {
            next_WP_loc.alt = current_loc.alt;
            next_WP_loc.flags.relative_alt = false;
            next_WP_loc.flags.terrain_alt = false;
        }
    }

    // convert relative alt to absolute alt
    if (next_WP_loc.flags.relative_alt) {
        next_WP_loc.flags.relative_alt = false;
        next_WP_loc.alt += home.alt;
    }

    // are we already past the waypoint? This happens when we jump
    // waypoints, and it can cause us to skip a waypoint. If we are
    // past the waypoint when we start on a leg, then use the current
    // location as the previous waypoint, to prevent immediately
    // considering the waypoint complete
    if (location_passed_point(current_loc, prev_WP_loc, next_WP_loc)) {
        gcs_send_text(MAV_SEVERITY_NOTICE, "Resetting previous waypoint");
        prev_WP_loc = current_loc;
    }

    // used to control FBW and limit the rate of climb
    // -----------------------------------------------
    set_target_altitude_location(next_WP_loc);

    // zero out our loiter vals to watch for missed waypoints
    loiter_angle_reset();

    setup_glide_slope();
    setup_turn_angle();

    loiter_angle_reset();
}

void Plane::set_guided_WP(void)
{
    if (g.loiter_radius < 0 || guided_WP_loc.flags.loiter_ccw) {
        loiter.direction = -1;
    } else {
        loiter.direction = 1;
    }

    // copy the current location into the OldWP slot
    // ---------------------------------------
    prev_WP_loc = current_loc;

    // Load the next_WP slot
    // ---------------------
    next_WP_loc = guided_WP_loc;

    // used to control FBW and limit the rate of climb
    // -----------------------------------------------
    set_target_altitude_current();

    update_flight_stage();
    setup_glide_slope();
    setup_turn_angle();

    loiter_angle_reset();
}

// run this at setup on the ground
// -------------------------------
void Plane::init_home()
{
    gcs_send_text(MAV_SEVERITY_INFO, "Init HOME");

    ahrs.set_home(gps.location());
    home_is_set = HOME_SET_NOT_LOCKED;
    Log_Write_Home_And_Origin();
    GCS_MAVLINK::send_home_all(gps.location());

    gcs_send_text_fmt(MAV_SEVERITY_INFO, "GPS alt: %lu", (unsigned long)home.alt);

    // Save Home to EEPROM
    mission.write_home_to_storage();

    // Save prev loc
    // -------------
    next_WP_loc = prev_WP_loc = home;
}

/*
  update home location from GPS
  this is called as long as we have 3D lock and the arming switch is
  not pushed
*/
void Plane::update_home()
{
    if (home_is_set == HOME_SET_NOT_LOCKED) {
        ahrs.set_home(gps.location());
        Log_Write_Home_And_Origin();
        GCS_MAVLINK::send_home_all(gps.location());
    }
    barometer.update_calibration();
}
