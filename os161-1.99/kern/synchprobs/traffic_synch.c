#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include "opt-A1.h"

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
#if OPT_A1
  static int status[4] = {0, 0, 0, 0};
  static int waits[4] = {0, 0, 0, 0};
  static int direct = -1;
  static struct cv *North;
  static struct cv *East;
  static struct cv *South;
  static struct cv *West;
  static struct lock *IntersectionLck;
#else
  static struct semaphore *intersectionSem;
#endif


/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void intersection_sync_init(void) {
  #if OPT_A1
    /* replace this default implementation with your own implementation */
    North = cv_create("North");
    if (North == NULL) {
      panic("Could not create North CV.");
    }

    East = cv_create("East");
    if (East == NULL) {
      panic("Could not create East CV.");
    }

    South = cv_create("South");
    if (South == NULL) {
      panic("Could not create South CV.");
    }

    West = cv_create("West");
    if (West == NULL) {
      panic("Could not create West CV.");
    }

    IntersectionLck = lock_create("Intersection");
    if (IntersectionLck == NULL) {
      panic("Could not create Intersection lock.");
    }
  #else
    intersectionSem = sem_create("intersectionSem",1);
    if (intersectionSem == NULL) {
      panic("could not create intersection semaphore");
    }
    return;
  #endif
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void intersection_sync_cleanup(void) {
  #if OPT_A1
    /* replace this default implementation with your own implementation */
    KASSERT(North != NULL);
    KASSERT(East != NULL);
    KASSERT(South != NULL);
    KASSERT(West != NULL);
    KASSERT(IntersectionLck != NULL);
    
    cv_destroy(North);
    cv_destroy(East);
    cv_destroy(South);
    cv_destroy(West);
    lock_destroy(IntersectionLck);
  #else
    KASSERT(intersectionSem != NULL);
    sem_destroy(intersectionSem);
  #endif
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void intersection_before_entry(Direction origin, Direction destination) {
  #if OPT_A1
    /* replace this default implementation with your own implementation */
    KASSERT(North != NULL);
    KASSERT(East != NULL);
    KASSERT(South != NULL);
    KASSERT(West != NULL);
    KASSERT(IntersectionLck != NULL);

    lock_acquire(IntersectionLck);

    if (origin == north) {
      // status[0] is north
      if ((status[1] > 0) || (status[2] > 0) || (status[3] > 0) ||
          (direct >= 1)) {
        waits[0] += 1;
        cv_wait(North, IntersectionLck);
      }
      if (direct < 0) {
        direct = 0;
      }
      status[0] += 1;
      waits[0] = 0;

    } else if (origin == east) {
      // status[1] is east
      if ((status[0] > 0) || (status[2] > 0) || (status[3] > 0) ||
          ((direct >= 0) && (direct != 1))) {
        waits[1] += 1;
        cv_wait(East, IntersectionLck);
      }
      if (direct < 0) {
        direct = 1;
      }
      status[1] += 1;
      waits[1] = 0;

    } else if (origin == south) {
      // status[2] is south
      if ((status[0] > 0) || (status[1] > 0) || (status[3] > 0) ||
          ((direct >= 0) && (direct != 2))) {
        waits[2] += 1;
        cv_wait(South, IntersectionLck);
      }
      if (direct < 0) {
        direct = 2;
      }
      status[2] += 1;
      waits[2] = 0;

    } else { // origin == west
      // status[3] is west
      if ((status[0] > 0) || (status[1] > 0) || (status[2] > 0) ||
          ((direct >= 0) && (direct <= 2))) {
        waits[3] += 1;
        cv_wait(West, IntersectionLck);
      }
      if (direct < 0) {
        direct = 3;
      }
      status[3] += 1;
      waits[3] = 0;
    }

    lock_release(IntersectionLck);
    (void)destination; /* avoid compiler complaint about unused parameter */
  #else
    (void)origin;  /* avoid compiler complaint about unused parameter */
    (void)destination; /* avoid compiler complaint about unused parameter */
    KASSERT(intersectionSem != NULL);
    P(intersectionSem);
  #endif
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void intersection_after_exit(Direction origin, Direction destination) {
  #if OPT_A1
    /* replace this default implementation with your own implementation */
    KASSERT(North != NULL);
    KASSERT(East != NULL);
    KASSERT(South != NULL);
    KASSERT(West != NULL);
    KASSERT(IntersectionLck != NULL);

    lock_acquire(IntersectionLck);

    if (origin == north) {
      KASSERT(status[0] > 0);
      status[0] -= 1;
    } else if (origin == east) {
      KASSERT(status[1] > 0);
      status[1] -= 1;
    } else if (origin == south) {
      KASSERT(status[2] > 0);
      status[2] -= 1;
    } else { // origin == west
      KASSERT(status[3] > 0);
      status[3] -= 1;
    }

    if ((status[0] == 0) && (status[1] == 0) &&
        (status[2] == 0) && (status[3] == 0)) {
      if ((waits[0] == 0) && (waits[1] == 0) &&
          (waits[2] == 0) && (waits[3] == 0)) {
        // Done
        direct = -1;
        lock_release(IntersectionLck);
        return;
      }
      if (origin == north) {
        if (waits[1] >= 1) {
          direct = 1;
          cv_broadcast(East, IntersectionLck);
        } else if (waits[2] >= 1) {
          direct = 2;
          cv_broadcast(South, IntersectionLck);
        } else { // waits[3]
          direct = 3;
          cv_broadcast(West, IntersectionLck);
        }
        
      } else if (origin == east) {
        if (waits[2] >= 1) {
          direct = 2;
          cv_broadcast(South, IntersectionLck);
        } else if (waits[3] >= 1) {
          direct = 3;
          cv_broadcast(West, IntersectionLck);
        } else { // waits[0]
          direct = 0;
          cv_broadcast(North, IntersectionLck);
        }

      } else if (origin == south) {
        if (waits[3] >= 1) {
          direct = 3;
          cv_broadcast(West, IntersectionLck);
        } else if (waits[0] >= 1) {
          direct = 0;
          cv_broadcast(North, IntersectionLck);
        } else { // waits[1]
          direct = 1;
          cv_broadcast(East, IntersectionLck);
        }

      } else { // origin == west
        if (waits[0] >= 1) {
          direct = 0;
          cv_broadcast(North, IntersectionLck);
        } else if (waits[1] >= 1) {
          direct = 1;
          cv_broadcast(East, IntersectionLck);
        } else { // waits[2]
          direct = 2;
          cv_broadcast(South, IntersectionLck);
        }
      }
    }

    lock_release(IntersectionLck);
    (void)destination; /* avoid compiler complaint about unused parameter */
  #else
    (void)origin;  /* avoid compiler complaint about unused parameter */
    (void)destination; /* avoid compiler complaint about unused parameter */
    KASSERT(intersectionSem != NULL);
    V(intersectionSem);
  #endif
}
