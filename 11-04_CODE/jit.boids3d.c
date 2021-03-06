/*
  
  This project is a Max external for Swarm-PI project called "jit.boids3d.c" and
  was adapted from Wesley Smith (2005), Eric Singer (2003), and Andre Sier (2007).
  The project was found and designated free for non-commercial use.
  
  Jack Truskowski and Grace Handler developed this project beginning in May 2015
  until December 2016 as an independent study at Bowdoin College under the advisory of
  Stephen Majercik and Frank Mauceri.
  
  */

/* 
 * HeaderDoc was used for commenting this project.
 !!! Option-Click on names of functions and variables to see more information
 */

#include "jit.common.h"
#include <math.h>

/*
 * Constants
 */
#define kMaxNeighbors 200
#define kMaxNeighborLines 272 //tested from max patch, it doesn't like rendering more lines than this
#define kMaxNumBoids 1000
#define MAX_FLOCKS 6 // Maximum number of flocks allowed in simulation

/*
  * Initial flight parameters
  * NOTE: These aren't really used, because the patcher is banged on startup and adds default paramters
  */
const int kBoidMaxAge = 1000;
const long kNumBoids = 0;
const long kNumNeighbors = 10;
const double kMinSpeed = 0.15;
const double kMaxSpeed = 0.25;
const double kCenterWeight = 0.25;
const double kAttractWeight	= 0.300;
const double kMatchWeight	= 0.100;
const double kSepWeight	= 0.10;
const double kSepDist = 1.0;
const double kDefaultSpeed = 0.100;
const double kInertiaFactor	= 0.20;
const double kAccelFactor = 0.100;
const double kNRadius = 0.25;
const double kFlyRectTop = 1.0;
const double kFlyRectLeft = -1.0;
const double kFlyRectBottom	= -1.0;
const double kFlyRectRight = 1.0;
const double kFlyRectFront = 1.0;
const double kFlyRectBack = -1.0;
const double kFlyRectScalingFactor = 10;

/*
  * NOTE: #define is used instead of strcuts for the sake of Max's Jitter object
  * because of the way attribute data types work.
  */

//use defines instead of structs in jitter object
//because of the way attribute data types work

/*
  * For Point3D and Velocity
  */
#define x 0
#define y 1
#define z 2

/*
  * For FlyRect
  */
#define left 0
#define right 1
#define top 2
#define bottom 3
#define front 4
#define back 5

/*!
  * @typedef Attractor
  * @brief Struct for one attractor object, which will be contained in LinkedList of boids
  */
typedef struct Attractor {
    struct Attractor *nextAttractor; // Pointer to next attractor
    double loc[3];
    double attractorRadius; // Attraction radius of attractor
    int id;
    int onlyAttractedFlockID; //-1 if all flocks feel the attractor, otherwise the ID of the only flock that will feel this attractor
} Attractor, *AttractorPtr;

/*
 Struct for a boid object
 Boids are stored in LinkedLists by flock in the _jit_boids3d struct
 
 TODO: add some sort of ID
 */
/*!
  * @typedef Boid
  * @brief Struct for one boid object, which will be contained in LinkedList of boids
 *  FIXME Add some sort of ID
  */
typedef struct Boid {
    int flockID; //
    int age;
    int globalID; //a unique identifier across all flocks
    double oldPos[3];
    double newPos[3];
    double oldDir[3];
    double newDir[3];
    double speed;
    long neighbor[kMaxNeighbors];
    double neighborDistSqr[kMaxNeighbors];
    struct Boid *nextBoid;
} Boid, *BoidPtr;


/*
 * @typedef NeighborLine
 * @brief Struct for a line between two neigh boids; stored in an array; output in 4th outlet
 */
typedef struct NeighborLine {
    
    float boidA[3];
    int aID; //boid A global id
    
    float boidB[3];
    int bID; //boid B global id
    
    int flockID[2]; //[boidAflockID, boidBflockID]
    
} NeighborLine, *NeighborLinePtr;

/*!
 * @typedef _jit_boids3d
 * @brief Struct for the actual jitter object holding LinkedList of boids, attractors, etc.
 */
typedef struct _jit_boids3d
{
    t_object ob;
    char mode;
    long number;
    long numAttractors;
    long neighbors;
    double flyrect[6]; // dimensions of the simulation
    long flyRectCount;
    char allowNeighborsFromDiffFlock; // bool, if boids can find neighbors that are in another flock
    double birthLoc[3]; // birth location of boids, default is {0,0,0}
    int newBoidID;
    
    // Flock specific paramters
    int boidCount[MAX_FLOCKS];
    int flockID[MAX_FLOCKS];
    double minspeed[MAX_FLOCKS];
    double maxspeed[MAX_FLOCKS];
    double center[MAX_FLOCKS];
    double attract[MAX_FLOCKS];
    double match[MAX_FLOCKS];
    double sepwt[MAX_FLOCKS];
    double sepdist[MAX_FLOCKS];
    double speed[MAX_FLOCKS];
    double inertia[MAX_FLOCKS];
    double accel[MAX_FLOCKS];
    double neighborRadius[MAX_FLOCKS];
    double age[MAX_FLOCKS];
    double tempCenterPt[3];
    long centerPtCount;
    
    NeighborLinePtr neighborhoodConnections[kMaxNeighborLines]; // Array to hold lines between neighbors
    long sizeOfNeighborhoodConnections;
    int drawingNeighbors; //boolean to avoid computing neighbor lines if we are not drawing neighbors
    
    BoidPtr flockLL[MAX_FLOCKS]; // Array holding at most 6 LinkedLists of flocks
    AttractorPtr attractorLL; // Array holding at most 6 LinkedLists of attractors
    
    int tempForStats[1]; //?
    
    // Setting angle of velocity
    double 			d2r; // Degrees --> Radians
    double			r2d; // Radians --> Degrees
    
} t_jit_boids3d;


/*
 Methods for the jitter object
 */
void *_jit_boids3d_class;
t_jit_err jit_boids3d_init(void);
t_jit_boids3d *jit_boids3d_new(void);
void freeFlocks(t_jit_boids3d *flockPtr);
t_jit_err jit_boids3d_matrix_calc(t_jit_boids3d *flockPtr, void *inputs, void *outputs);

void jit_boids3d_calculate_ndim(t_jit_boids3d *flockPtr, long dimcount, long *dim, long planecount,
                                t_jit_matrix_info *out_minfo, char *bop);

//Attribute methods
/*
 These are the method that will get called when a message is received from the Max patch
 ie) [pak age 0. 0.] -> will call the jit_boids3d_age() method
 */
t_jit_err jit_boids3d_neighbors(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_minspeed(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_nradius(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_inertia(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_number(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_maxspeed(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_center(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_attract(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_match(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_sepwt(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_sepdist(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_speed(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_accel(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_age(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_attractpt(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_addattractor(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_deleteattractor(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_birthloc(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv);
t_jit_err jit_boids3d_stats(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv); //posts various stats to the max console
t_jit_err jit_boids3d_drawingneighbors(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv); //0/1 if the max patch wants to draw neighbors


//Initialization methods
void InitFlock(t_jit_boids3d *flockPtr);
BoidPtr InitLL(t_jit_boids3d *flockPtr, long numBoids, int flockID);
BoidPtr InitBoid(t_jit_boids3d *flockPtr);
AttractorPtr InitAttractor(t_jit_boids3d *flockPtr);
NeighborLinePtr InitNeighborhoodLine(t_jit_boids3d *flockPtr, BoidPtr theBoid, BoidPtr theOtherBoid);

//Methods for running the simulation
void FlightStep(t_jit_boids3d *flockPtr);
void CalcFlockCenterAndNeighborVel(t_jit_boids3d *flockPtr, BoidPtr theBoid, double *matchNeighborVel, double *separationNeighborVel);
void SeekPoint(t_jit_boids3d *flockPtr, BoidPtr theBoid, double *seekPt, double* seekDir);
void SeekAttractors(t_jit_boids3d *flockPtr, BoidPtr theBoid, double* seekDir);
void AvoidWalls(t_jit_boids3d *flockPtr, BoidPtr theBoid, double *wallVel);
char InFront(BoidPtr theBoid, BoidPtr neighbor);
int CalcNumBoids(t_jit_boids3d *flockPtr);

//Helper methods
void NormalizeVelocity(double *direction);
double RandomInt(double minRange, double maxRange);
double DistSqrToPt(double *firstPoint, double *secondPoint);


/*
    Initializes the jitter object
 */
t_jit_err jit_boids3d_init(void)
{
    long attrflags=0;
    t_jit_object *attr,*mop,*o, *o2, *o3, *o4; //o, o2, and o3 are the 3 outlets. Mop stands for a matrix in jitter
    t_symbol *atsym;
    
    atsym = gensym("jit_attr_offset");
    
    //make a class and tell it the methods it will use to initialize and free
    _jit_boids3d_class = jit_class_new("jit_boids3d",(method)jit_boids3d_new,(method)freeFlocks,
                                       sizeof(t_jit_boids3d),0L);
    
    //add mop
    mop = jit_object_new(_jit_sym_jit_mop,0,4); //object will have 0 inlets and 3 outlets
    o = jit_object_method(mop,_jit_sym_getoutput,1); //first outlet
    o2 = jit_object_method(mop,_jit_sym_getoutput,2); //second outlet
    o3 = jit_object_method(mop,_jit_sym_getoutput,3); //third outlet
    o4 = jit_object_method(mop,_jit_sym_getoutput,4); //fourth outlet
    jit_attr_setlong(o,_jit_sym_dimlink,0);
    jit_attr_setlong(o2,_jit_sym_dimlink,0);
    jit_attr_setlong(o3,_jit_sym_dimlink,0);
    jit_attr_setlong(o4,_jit_sym_dimlink,0);
    
    
    jit_class_addadornment(_jit_boids3d_class,mop);
    //add methods
    jit_class_addmethod(_jit_boids3d_class, (method)jit_boids3d_matrix_calc, 		"matrix_calc", 		A_CANT, 0L);
    jit_class_addmethod(_jit_boids3d_class, (method)InitBoid, 				"init_boid", 			A_USURP_LOW, 0L);
    
    //add attributes
    attrflags = JIT_ATTR_GET_DEFER_LOW | JIT_ATTR_SET_USURP_LOW;
    
    //mode
    attr = jit_object_new(atsym,"mode",_jit_sym_char,attrflags,
                          (method)0L,(method)0L,calcoffset(t_jit_boids3d,mode));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //allow boids from diff flocks
    attr = jit_object_new(atsym,"diffFlock",_jit_sym_char,attrflags,
                          (method)0L,(method)0L,calcoffset(t_jit_boids3d,allowNeighborsFromDiffFlock));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //neighbor radius
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"nradius",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_nradius, calcoffset(t_jit_boids3d,neighborRadius));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //number
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"number",_jit_sym_long, 6, attrflags,
                          (method)0L,(method)jit_boids3d_number,calcoffset(t_jit_boids3d,number));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //neighbors
    attr = jit_object_new(atsym,"neighbors",_jit_sym_long,attrflags,
                          (method)0L,(method)jit_boids3d_neighbors,calcoffset(t_jit_boids3d,neighbors));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //flyrect
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"flyrect",_jit_sym_float64,6,attrflags,
                          (method)0L,(method)0L,calcoffset(t_jit_boids3d,flyRectCount),calcoffset(t_jit_boids3d,flyrect));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //minspeed
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"minspeed",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_minspeed,calcoffset(t_jit_boids3d,minspeed));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //maxspeed
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"maxspeed",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_maxspeed,calcoffset(t_jit_boids3d,maxspeed));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //center
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"center",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_center,calcoffset(t_jit_boids3d,center));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //attract
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"attract",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_attract,calcoffset(t_jit_boids3d,attract));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //match
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"match",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_match,calcoffset(t_jit_boids3d,match));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //separation weight
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"sepwt",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_sepwt,calcoffset(t_jit_boids3d,sepwt));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //separation distance
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"sepdist",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_sepdist,calcoffset(t_jit_boids3d,sepdist));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //speed
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"speed",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_speed,calcoffset(t_jit_boids3d,speed));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //inertia
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"inertia",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_inertia,calcoffset(t_jit_boids3d,inertia));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //accel
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"accel",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_accel,calcoffset(t_jit_boids3d,accel));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //attractpt
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"attractpt",_jit_sym_float64, 4, attrflags,
                          (method)0L,(method)jit_boids3d_attractpt,calcoffset(t_jit_boids3d,numAttractors));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //age
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"age",_jit_sym_float64,2,attrflags,
                          (method)0L,(method)jit_boids3d_age,calcoffset(t_jit_boids3d,speed));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //add attractor
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"addattractor",_jit_sym_long,2,attrflags,
                          (method)0L,(method)jit_boids3d_addattractor,calcoffset(t_jit_boids3d,numAttractors));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //delete attractor
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"deleteattractor",_jit_sym_long,2,attrflags,
                          (method)0L,(method)jit_boids3d_deleteattractor,calcoffset(t_jit_boids3d,numAttractors));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //birthpt
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"birthloc",_jit_sym_float64, 4, attrflags,
                          (method)0L,(method)jit_boids3d_birthloc,calcoffset(t_jit_boids3d,birthLoc));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //stats
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"stats",_jit_sym_float64, 0, attrflags,
                          (method)0L,(method)jit_boids3d_stats,calcoffset(t_jit_boids3d,tempForStats));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    //doing neighbor lines
    attr = jit_object_new(_jit_sym_jit_attr_offset_array,"drawingneighbors",_jit_sym_long,1,attrflags,
                          (method)0L,(method)jit_boids3d_drawingneighbors,calcoffset(t_jit_boids3d,drawingNeighbors));
    jit_class_addattr(_jit_boids3d_class,attr);
    
    
    jit_class_register(_jit_boids3d_class); //register the class with Max
    
    return JIT_ERR_NONE;
    
}


//
//
//      MARK: Boids Attribute Methods
//
//

/*!
    @brief Updates the position of an attractor
    @param flockPtr a pointer to the flock object
    @param argv Arguments coming from the max patch:
            [0] = new x position
            [1] = new y position
            [2] = new z position
            [3] = new radius of attractor
            [4] = id of the attractor to be updated
 */
t_jit_err jit_boids3d_attractpt(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int attractorID = (int)jit_atom_getfloat(argv+4);
    AttractorPtr iterator = flockPtr->attractorLL;
    while (iterator){
        if(attractorID == iterator->id){
            //this is the attractor we want to modify
            if(iterator){
                iterator->loc[0] = (double)jit_atom_getfloat(argv);
                iterator->loc[1] = (double)jit_atom_getfloat(argv+1);
                iterator->loc[2] = (double)jit_atom_getfloat(argv+2);
                iterator->attractorRadius = (double)jit_atom_getfloat(argv+3);
            }
            return JIT_ERR_NONE;
        }
        iterator = iterator->nextAttractor;
    }
    
    return JIT_ERR_NONE;
}



/*!
    @brief Adds an attractor at the origin
    @param argv the ID of the new attractor
 */
t_jit_err jit_boids3d_addattractor(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    //grab the ID of the new attractor
    int newID = (int)jit_atom_getlong(argv);
    
    //initialize an attractor
    AttractorPtr iterator = flockPtr->attractorLL;
    AttractorPtr newAttractor = InitAttractor(flockPtr);
    if(newID == 0){
        newAttractor->onlyAttractedFlockID = 0;
    }
    
    flockPtr->numAttractors++;
    
    //no attractors exist
    if(!iterator){
        newAttractor->id = newID;
        flockPtr->attractorLL = newAttractor;
        flockPtr->attractorLL->nextAttractor = NULL;
        return JIT_ERR_NONE;
    }
    
    //at least one attractor already exists
    //check if there already exists an attractor with newID
    int idAlreadyExists = 0;
    int maxID = 0;
    while(iterator){
        if(iterator->id > maxID){
            maxID = iterator->id;
        }
        if(iterator->id == newID){
            idAlreadyExists = 1;
            break;
        }
        iterator = iterator->nextAttractor;
    }
    iterator = flockPtr->attractorLL;
    
    //add to front
    if(idAlreadyExists == 0){
        newAttractor->id = newID;
    }else{
        newAttractor->id = maxID+1;
    }
    newAttractor->nextAttractor = iterator;
    flockPtr->attractorLL = newAttractor;
    return JIT_ERR_NONE;
}


/*!
 @brief Updates the drawingNeighbors boolean
 @param argv boolean int of whether neighbor lines should be drawn or not
 */
t_jit_err jit_boids3d_drawingneighbors(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int draw = (int)jit_atom_getlong(argv);
    flockPtr->drawingNeighbors = draw;
    
    return JIT_ERR_NONE;
}


/*!
    @brief Deletes an attractor with given ID
    @param argv the ID of the attractor to be deleted
 */
t_jit_err jit_boids3d_deleteattractor(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int attractorID = (int)jit_atom_getlong(argv);
    AttractorPtr iterator = flockPtr->attractorLL;
    AttractorPtr prev = iterator;
    
    //iterate thru the LL of attractors to find the right one
    while(iterator){
        if(iterator->id == attractorID){
            //this is the attractor to delete
            if(prev == iterator){ //first attractor
                iterator = iterator->nextAttractor;
                free(prev);
                flockPtr->attractorLL = iterator;
                
            }else{
                
                //middle or end
                prev->nextAttractor = iterator->nextAttractor;
                free(iterator);
            }
            
            //update numAttractors and mark the LL NULL if necessary
            flockPtr->numAttractors--;
            if(flockPtr->numAttractors <= 0){
                flockPtr->attractorLL = NULL;
            }
            return JIT_ERR_NONE;
            
        }
        prev = iterator;
        iterator = iterator->nextAttractor;
    }
    
    //couldn't find this attractor
    return JIT_ERR_NONE;
}


/*
 Following methods update flock-specific attributes
 */

//---NOT CURRENTLY USED---
t_jit_err jit_boids3d_neighbors(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    flockPtr->neighbors = (double)MIN(jit_atom_getfloat(argv), kMaxNeighbors);
    return JIT_ERR_NONE;
}
//--------------------------

t_jit_err jit_boids3d_nradius(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->neighborRadius[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.0);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_minspeed(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->minspeed[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_maxspeed(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->maxspeed[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_center(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->center[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_attract(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->attract[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_match(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->match[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_sepwt(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->sepwt[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_sepdist(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->sepdist[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_speed(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->speed[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_inertia(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    double val = (double)jit_atom_getfloat(argv);
    int flockID =(int)jit_atom_getfloat(argv+1);
    
    if(val == 0.0)
        flockPtr->inertia[flockID] = 0.000001;
    else
        flockPtr->inertia[flockID] = val;
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_accel(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->accel[flockID] = (double)MAX(jit_atom_getfloat(argv), 0.000001);
    return JIT_ERR_NONE;
}

t_jit_err jit_boids3d_age(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int flockID = (int)jit_atom_getfloat(argv+1);
    flockPtr->age[flockID] = (double)jit_atom_getfloat(argv);
    return JIT_ERR_NONE;
}


/*!
    @brief Updates the position of the birth location
    @param argv argv[0]/[1]/[2] = The xyz of the new position
 */
t_jit_err jit_boids3d_birthloc(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv){
    
    flockPtr->birthLoc[x] = (double)jit_atom_getfloat(argv);
    flockPtr->birthLoc[y] = (double)jit_atom_getfloat(argv+1);
    flockPtr->birthLoc[z] = (double)jit_atom_getfloat(argv+2);
    
    return JIT_ERR_NONE;
}


/*!
    @brief Adds or deletes a specified number of boids from a flock
    @param argv 0-5 = change for each flock. 6 = total change in number of boids
    @warning Changes in number of boids and total change may be negative if boids are being deleted
 */
t_jit_err jit_boids3d_number(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv)
{
    int boidChanges[MAX_FLOCKS]; //number of boids being deleted from each flock
    
    int newNumBoids; //new total number of boids across all flocks
    newNumBoids = jit_atom_getlong(argv+6);
    
    for (int i=0; i<MAX_FLOCKS; i++){
        boidChanges[i] = (int)jit_atom_getlong(argv+i); //new total boids for the ith flock
    }
    
    //make sure the number of boids in at least one flock is being changed
    int changed = 0;
    int totalChanges = 0;
    for(int i=0; i<MAX_FLOCKS; i++){
        if (boidChanges[i] != 0){
            totalChanges += boidChanges[i];
            changed = 1;
        }
    }
    if(changed == 0 || totalChanges+CalcNumBoids(flockPtr)>kMaxNumBoids){
        return NULL;
    }
    
    //iterate thru flocks and update boids
    for (int i=0; i<MAX_FLOCKS; i++){
        
        if(boidChanges[i] == 0) continue; //no changes in this flock
        
        else if(boidChanges[i] < 0){ //we're deleting boids
            BoidPtr iterator = flockPtr->flockLL[i];
            BoidPtr toBeDeleted = iterator;
            while (boidChanges[i] < 0){
                if(!toBeDeleted){
                    break;
                }
                iterator = iterator->nextBoid;
                free(toBeDeleted);
                toBeDeleted = iterator;
                flockPtr->flockLL[i] = iterator;
                flockPtr->boidCount[i]--; //update the number of boids in flock
                boidChanges[i]++;
            }
        }else{ //we're adding boids
            for (int j=0; j<boidChanges[i]; j++){
                
                //initialize a new boid and add it to the front of the LL
                BoidPtr newBoid = InitBoid(flockPtr);
                if(!newBoid){
                    return NULL;
                }
                newBoid->nextBoid = flockPtr->flockLL[i];
                flockPtr->flockLL[i] = newBoid;
                newBoid->flockID = i;
                
                flockPtr->boidCount[i]++; //update the number of boids in flock
            }
        }
    }
    
    return 0;
}


/*!
    @brief Posts various statistics to the max console for debugging purposes
    @discussion Stats include:
                    Flock Sizes,
                    Attractor Location,
                    Birth Location,
                    Number of Neighbor Lines,
 */
t_jit_err jit_boids3d_stats(t_jit_boids3d *flockPtr, void *attr, long argc, t_atom *argv){
    
    post(" - - STATS - - ");
    
    //flock size information
    post("Flock Sizes:");
    for(int i=0; i<MAX_FLOCKS; i++){
        post("   %d: %d boids", i, flockPtr->boidCount[i]);
    }
    
    //attractor information
    if(flockPtr->numAttractors > 0){
        post("Attractors:");
        AttractorPtr iterator = flockPtr->attractorLL;
        while(iterator){
            post("   ID: %d,  Location: (%0.2f, %0.2f, %0.2f), Strength: %0.2f", iterator->id, iterator->loc[x], iterator->loc[y], iterator->loc[z], iterator->attractorRadius);
            iterator = iterator->nextAttractor;
        }
    }else{
        post("No Attractors.");
    }
    
    //birth location
    post("Birth Location: (%0.2f, %0.2f, %0.2f)", flockPtr->birthLoc[x], flockPtr->birthLoc[y], flockPtr->birthLoc[z]);
    
    //neighbor connections
    post("Number of Neighbor Lines: %d/%d", flockPtr->sizeOfNeighborhoodConnections, kMaxNeighborLines);
    
    post("Largest boid ID: %d", flockPtr->newBoidID);
    
    post("- - - - - - -");
    
    return 0;
}


//
//
//      MARK: Methods for output
//
//

/*
 Prepares the output matrix and sends it back to the max patch
 */
t_jit_err jit_boids3d_matrix_calc(t_jit_boids3d *flockPtr, void *inputs, void *outputs)
{
    
    //do a step in the simulation
    FlightStep(flockPtr);
    
    t_jit_err err=JIT_ERR_NONE;
    long out_savelock, out2_savelock, out3_savelock, out4_savelock; //if there is a problem, saves and locks the output matricies
    t_jit_matrix_info out_minfo, out2_minfo, out3_minfo, out4_minfo;
    char *out_bp, *out2_bp, *out3_bp, *out4_bp;
    long i,dimcount,planecount,dim[JIT_MATRIX_MAX_DIMCOUNT]; //dimensions and planes for the first output matrix
    void *out_matrix, *out2_matrix, *out3_matrix, *out4_matrix;
    
    out_matrix = jit_object_method(outputs,_jit_sym_getindex,0);
    out2_matrix = jit_object_method(outputs,_jit_sym_getindex,1);
    out3_matrix     = jit_object_method(outputs, _jit_sym_getindex, 2);
    out4_matrix     = jit_object_method(outputs, _jit_sym_getindex, 3);
    
    if (flockPtr&&out_matrix&&out2_matrix&&out3_matrix&&out4_matrix) {
        out_savelock = (long) jit_object_method(out_matrix,_jit_sym_lock,1);
        out2_savelock = (long) jit_object_method(out2_matrix,_jit_sym_lock,1);
        out3_savelock = (long) jit_object_method(out3_matrix, _jit_sym_lock,1);
        out4_savelock = (long) jit_object_method(out4_matrix, _jit_sym_lock,1);
        
        jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo); //assign the out_infos to their cooresponding out matrix
        jit_object_method(out2_matrix,_jit_sym_getinfo,&out2_minfo);
        jit_object_method(out3_matrix,_jit_sym_getinfo, &out3_minfo);
        jit_object_method(out4_matrix,_jit_sym_getinfo, &out4_minfo);
        
        int numBoids = CalcNumBoids(flockPtr);
        
        //dimensions of the output matrix (number of boids x 1)
        out_minfo.dim[0] = numBoids;
        out_minfo.dim[1] = 1;
        out_minfo.type = _jit_sym_float32; //outputting floating point numbers
        
        //(number of flocks x 1)
        out2_minfo.dim[0] = MAX_FLOCKS;
        out2_minfo.dim[1] = 1;
        out2_minfo.type = _jit_sym_float32; //outputting floating point numbers
        out2_minfo.planecount = 1;
        
        //dimensions of attractor output matrix (number of attractors x 1)
        out3_minfo.dim[0] = flockPtr->numAttractors;
        out3_minfo.dim[1] = 1;
        out3_minfo.type = _jit_sym_float32; //outputting floating point numbers
        out3_minfo.planecount = 5; //xyz, id, attractorRadius
        
        //dimensions of the neighborhood line connecting matrix
        out4_minfo.dim[0] = flockPtr->sizeOfNeighborhoodConnections;
        out4_minfo.dim[1] = 1;
        out4_minfo.type = _jit_sym_float32;
        out4_minfo.planecount = 9;
        
        //output the correct mode
        switch(flockPtr->mode) { // newpos
            case 0:
                out_minfo.planecount = 4;
                break;
            case 1: //newpos + oldpos
                out_minfo.planecount = 7;
                break;
            case 2://newpos +  oldpos + speed-azimuth-elevation
                out_minfo.planecount = 10;
                break;
        }
        
        //for some reason, 2 of these in a row are required
        jit_object_method(out_matrix,_jit_sym_setinfo,&out_minfo);
        jit_object_method(out_matrix,_jit_sym_getinfo,&out_minfo);
        
        jit_object_method(out2_matrix,_jit_sym_setinfo,&out2_minfo);
        jit_object_method(out2_matrix,_jit_sym_getinfo,&out2_minfo);
        
        jit_object_method(out3_matrix,_jit_sym_setinfo,&out3_minfo);
        jit_object_method(out3_matrix,_jit_sym_getinfo,&out3_minfo);
        
        jit_object_method(out4_matrix,_jit_sym_setinfo,&out4_minfo);
        jit_object_method(out4_matrix,_jit_sym_getinfo,&out4_minfo);
        
        jit_object_method(out_matrix,_jit_sym_getdata,&out_bp);
        jit_object_method(out2_matrix,_jit_sym_getdata,&out2_bp);
        jit_object_method(out3_matrix,_jit_sym_getdata,&out3_bp);
        jit_object_method(out4_matrix,_jit_sym_getdata,&out4_bp);
        
        //something went wrong, handle the error
        if (!out_bp || !out2_bp || !out3_bp || !out4_bp) {
            err=JIT_ERR_INVALID_OUTPUT;
            goto out;
        }
        
        //populate the second outlet matrix with data
        float *out2_data = (float*)out2_bp;
        for(int i=0; i<MAX_FLOCKS; i++){
            out2_data[0] = flockPtr->boidCount[i];
            out2_data+=1;
        }
        
        //populate the 3rd outlet with data
        float *out3_data = (float*)out3_bp;
        AttractorPtr iterator = flockPtr->attractorLL;
        while(iterator){
            out3_data[0] = iterator->loc[0];
            out3_data[1] = iterator->loc[1];
            out3_data[2] = iterator->loc[2];
            out3_data[3] = iterator->id;
            out3_data[4] = iterator->attractorRadius;
            
            out3_data += 5; //planecount
            
            iterator=iterator->nextAttractor;
        }
        
        //populate the 4th outlet with data
        float *out4_data = (float*)out4_bp;
        
        for(int i=0; i<flockPtr->sizeOfNeighborhoodConnections; i++){
            
            out4_data[0] = flockPtr->neighborhoodConnections[i]->boidA[x];
            out4_data[1] = flockPtr->neighborhoodConnections[i]->boidA[y];
            out4_data[2] = flockPtr->neighborhoodConnections[i]->boidA[z];
            
            out4_data[3] = flockPtr->neighborhoodConnections[i]->boidB[x];
            out4_data[4] = flockPtr->neighborhoodConnections[i]->boidB[y];
            out4_data[5] = flockPtr->neighborhoodConnections[i]->boidB[z];
            
            out4_data[6] = flockPtr->neighborhoodConnections[i]->flockID[0];
            out4_data[7] = flockPtr->neighborhoodConnections[i]->flockID[1];
            
            out4_data[8] = flockPtr->sizeOfNeighborhoodConnections; //TODO: this is a little hacky, should not need to dedicate a whole plane to this
            
            out4_data += 9; //planecount
        }
        
        
        
        //get dimensions/planecount
        dimcount   = out_minfo.dimcount;
        planecount = out_minfo.planecount;
        
        for (i=0;i<dimcount;i++) {
            dim[i] = out_minfo.dim[i];
        }
        
        //populate the first outlet matrix with data
        jit_boids3d_calculate_ndim(flockPtr, dimcount, dim, planecount, &out_minfo, out_bp);
        
    } else {
        return JIT_ERR_INVALID_PTR;
    }
    
out: //output the matrix
    jit_object_method(out_matrix,gensym("lock"),out_savelock);
    return err;
}

/*
 Populates the first outlet matrix with the data (boids x,y,z etc)
 */
void jit_boids3d_calculate_ndim(t_jit_boids3d *flockPtr, long dimcount, long *dim, long planecount,
                                t_jit_matrix_info *out_minfo, char *bop)
{
    
    float *fop;
    double 	tempNew_x, tempNew_y, tempNew_z;
    double 	tempOld_x, tempOld_y, tempOld_z;
    double	delta_x, delta_y, delta_z, azi, ele, speed;
    
    fop = (float *)bop; //contains the data
    
    //pick the correct mode (to get the appropriate number of planes)
    switch(flockPtr->mode) { // newpos
        case 0:
            for (int i=0; i<MAX_FLOCKS; i++){
                BoidPtr iterator = flockPtr->flockLL[i];
                
                while (iterator){ //iterate thru the boids in the flock and add their info to the matrix
                    fop[0] = iterator->newPos[x];
                    fop[1] = iterator->newPos[y];
                    fop[2] = iterator->newPos[z];
                    fop[3] = iterator->flockID;
                    
                    fop += planecount;
                    
                    iterator = iterator->nextBoid;
                }
            }
            break;
        case 1:
            for (int i=0; i<MAX_FLOCKS; i++){
                BoidPtr iterator = flockPtr->flockLL[i];
                
                while (iterator){ //iterate thru the boids in the flock and add their info to the matrix
                    fop[0] = iterator->newPos[x];
                    fop[1] = iterator->newPos[y];
                    fop[2] = iterator->newPos[z];
                    fop[3] = iterator->flockID;
                    fop[4] = iterator->oldPos[x];
                    fop[5] = iterator->oldPos[y];
                    fop[6] = iterator->oldPos[z];
                    
                    fop += planecount;
                    
                    iterator = iterator->nextBoid;
                }
            }
            break;
        case 2:
            for (int i=0; i<MAX_FLOCKS; i++){
                BoidPtr iterator = flockPtr->flockLL[i];
                
                while (iterator){ //iterate thru the boids in the flock and add their info to the matrix
                    tempNew_x = iterator->newPos[x];
                    tempNew_y = iterator->newPos[y];
                    tempNew_z = iterator->newPos[z];
                    tempOld_x = iterator->oldPos[x];
                    tempOld_y = iterator->oldPos[y];
                    tempOld_z = iterator->oldPos[z];
                    
                    delta_x = tempNew_x - tempOld_x;
                    delta_y = tempNew_y - tempOld_y;
                    delta_z = tempNew_z - tempOld_z;
                    azi = jit_math_atan2(delta_z, delta_x) * flockPtr->r2d;
                    ele = jit_math_atan2(delta_y, delta_x) * flockPtr->r2d;
                    speed = jit_math_sqrt(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);
                    
                    fop[0] = tempNew_x;
                    fop[1] = tempNew_y;
                    fop[2] = tempNew_z;
                    fop[3] = iterator->flockID;
                    fop[4] = tempOld_x;
                    fop[5] = tempOld_y;
                    fop[6] = tempOld_z;
                    fop[7] = speed;
                    fop[8] = azi;
                    fop[9] = ele;
                    
                    fop += planecount;
                    
                    iterator = iterator->nextBoid; //move to next boid
                }
            }
            break;
    }
}


//
//
//  MARK: Internal methods to the external for updating boids - Max patch does not interact directly with these
//
//


/*!
    @brief This method performs the velocity and position updates for all the boids
    @param flockPtr A pointer to the flocks object
 */
void FlightStep(t_jit_boids3d *flockPtr)
{
    //All velocities start out at 0, otherwise weird boid velocities may happen
    double			goCenterVel[3] = {0,0,0};
    double			goAttractVel[3] = {0,0,0};
    double			matchNeighborVel[3] = {0,0,0};
    double			separationNeighborVel[3] = {0,0,0};
    
    //Initialize the lines
    flockPtr->sizeOfNeighborhoodConnections = 0;
    
    //get every boid from every flock
    for (int i=0; i<MAX_FLOCKS; i++){
        BoidPtr iterator = flockPtr->flockLL[i];
        BoidPtr prevBoid = NULL;
        
        while(iterator){ //grab every boid from this flock
            
            //update age and check if it's this boid's time to die
            iterator->age++;
            if(iterator->age > flockPtr->age[iterator->flockID] && flockPtr->age[iterator->flockID] != -1){
                
                //TODO: put the boid's ID in a LL so it can be reused (and IDS don't get arbitrarily large)
                
                BoidPtr deletor = iterator;
                
                //delete the boid and continue
                if(!prevBoid){ //this boid is at the head of the linked list
                    deletor = iterator;
                    iterator = iterator->nextBoid;
                    free(deletor);
                    flockPtr->flockLL[i] = iterator;
                    
                }else{ //this boid is somewhere in the middle of the LL
                    iterator = iterator->nextBoid;
                    free(deletor);
                    prevBoid->nextBoid = iterator;
                }
                
                //update boid pointers and count and move to next boid
                flockPtr->boidCount[i]--;
                continue;
                
            }
            
            //save position and velocity
            iterator->oldPos[x] = iterator->newPos[x];
            iterator->oldPos[y] = iterator->newPos[y];
            iterator->oldPos[z] = iterator->newPos[z];
            
            iterator->oldDir[x] = iterator->newDir[x];
            iterator->oldDir[y] = iterator->newDir[y];
            iterator->oldDir[z] = iterator->newDir[z];
            
            //calculate velocity updates
            int flockID = iterator->flockID;
            
            CalcFlockCenterAndNeighborVel(flockPtr, iterator, matchNeighborVel,  separationNeighborVel);
            
            //update velocity to include centering and attracting instincts
            SeekPoint(flockPtr, iterator, flockPtr->tempCenterPt, goCenterVel);
            
            //Seek the attractors
            SeekAttractors(flockPtr, iterator, goAttractVel);
            
            // compute resultant velocity using weights and inertia
            iterator->newDir[x] = flockPtr->inertia[flockID] * (iterator->oldDir[x]) +
            (flockPtr->center[flockID] * goCenterVel[x] +
             flockPtr->attract[flockID] * goAttractVel[x] +
             flockPtr->match[flockID] * matchNeighborVel[x] +
             flockPtr->sepwt[flockID] * separationNeighborVel[x]) / flockPtr->inertia[flockID];
            iterator->newDir[y] = flockPtr->inertia[flockID] * (iterator->oldDir[y]) +
            (flockPtr->center[flockID] * goCenterVel[y] +
             flockPtr->attract[flockID] * goAttractVel[y] +
             flockPtr->match[flockID] * matchNeighborVel[y] +
             flockPtr->sepwt[flockID] * separationNeighborVel[y]) / flockPtr->inertia[flockID];
            iterator->newDir[z] = flockPtr->inertia[flockID] * (iterator->oldDir[z]) +
            (flockPtr->center[flockID] * goCenterVel[z] +
             flockPtr->attract[flockID] * goAttractVel[z] +
             flockPtr->match[flockID] * matchNeighborVel[z] +
             flockPtr->sepwt[flockID] * separationNeighborVel[z]) / flockPtr->inertia[flockID];
            
            //calculate the speed by finding the magnitude of all velocity components
            double newSpeed = sqrt(pow(iterator->newDir[x],2) + pow(iterator->newDir[y],2) + pow(iterator->newDir[z],2));
            
            NormalizeVelocity(iterator->newDir);	// normalize velocity so its length is unified
            
            // set to newSpeed bounded by minspeed and maxspeed
            if ((newSpeed >= flockPtr->minspeed[flockID]) &&
                (newSpeed <= flockPtr->maxspeed[flockID]))
                iterator->speed = newSpeed;
            else if (newSpeed > flockPtr->maxspeed[flockID])
                iterator->speed = flockPtr->maxspeed[flockID];
            else
                iterator->speed = flockPtr->minspeed[flockID];
            
            
            
            //bounce back from walls if the boid is beyond the limit of the flyrect
            AvoidWalls(flockPtr, iterator, iterator->newDir);
            
            // calculate new position, applying speed
            iterator->newPos[x] += iterator->newDir[x] * (0.5*iterator->speed) * (flockPtr->speed[flockID] / 100.0);
            iterator->newPos[y] += iterator->newDir[y] * (0.5*iterator->speed) * (flockPtr->speed[flockID] / 100.0);
            iterator->newPos[z] += iterator->newDir[z] * (0.5*iterator->speed) * (flockPtr->speed[flockID] / 100.0);
            
            //move to next boid
            prevBoid = iterator;
            iterator = iterator->nextBoid;
            
        }
        
    }
}


/*!
    @brief Calculates the center of a flock and saves it in flockPtr->centerPt
                Computes the avoid and matching of neighbor velocities
    @param flockPtr A pointer to the flocks object
    @param theBoid  The boid object that the calculations are performed for
    @param matchNeighborVel A reference to the matching velocity array in FlightStep()
    @param separationNeighborVel A reference to a separation velocity array in FlightStep()
 */
void CalcFlockCenterAndNeighborVel(t_jit_boids3d *flockPtr, BoidPtr theBoid, double *matchNeighborVel, double *separationNeighborVel)
{
    //TODO: avoid speed is never used
    
    
    //Variables for centering
    int flockID = theBoid->flockID;
    double totalH = 0, totalV = 0, totalD = 0;
    
    //Matching
    matchNeighborVel[x] = 0;
    matchNeighborVel[y] = 0;
    matchNeighborVel[z] = 0;
    
    //Variables for avoidance
    double avoidSpeed = theBoid->speed;
    int neighborsCount = 0; //counter to keep track of how many neighbors we've found
    
    //int startAddingBoidLines = 0;
    
    for(int i=0; i<MAX_FLOCKS; i++){ //grab every boid
        
        BoidPtr iterator = flockPtr->flockLL[i];
        
        while(iterator){
            
            double dist = sqrt(DistSqrToPt(theBoid->oldPos, iterator->oldPos));
            
            if(dist < flockPtr->neighborRadius[flockID] && dist > 0.0 && neighborsCount < kMaxNeighbors){ //check if this boid is close enough to be a neighbor
                
                //check to ensure this boid is allowed / in same flock
                if (flockPtr->allowNeighborsFromDiffFlock == 0 && iterator->flockID != flockID){
                    iterator = iterator->nextBoid;
                    continue;
                }
                
                //this boid is a neighbor
                
                //TODO: populate neighborhoodLines here
                
                //centering
                neighborsCount++;
                totalH += iterator->oldPos[x];
                totalV += iterator->oldPos[y];
                totalD += iterator->oldPos[z];
                
                //matching
                matchNeighborVel[x] += iterator->oldDir[x];
                matchNeighborVel[y] += iterator->oldDir[y];
                matchNeighborVel[z] += iterator->oldDir[z];
                
                //separation
                if(dist < flockPtr->sepdist[flockID]){
                    separationNeighborVel[x] += (theBoid->oldPos[x] - iterator->oldPos[x])/dist;
                    separationNeighborVel[y] += (theBoid->oldPos[y] - iterator->oldPos[y])/dist;
                    separationNeighborVel[z] += (theBoid->oldPos[z] - iterator->oldPos[z])/dist;
                }
                
                if (InFront((theBoid), iterator)) {	// adjust speed
                    avoidSpeed /= (flockPtr->accel[flockID] / 100.0);
                }
                else {
                    avoidSpeed *= (flockPtr->accel[flockID] / 100.0);
                }
                
                //Check if a line needs to be drawn between these boids
                if(flockPtr->sizeOfNeighborhoodConnections < kMaxNeighborLines && flockPtr->drawingNeighbors) {
                   
                    int lineAlreadyExists = 0;
                    
                    //Check to see if this line has already been added from another boid
                    //TODO: improve efficiency so its not ~O(n^2)?
                    for(int i=0; i<flockPtr->sizeOfNeighborhoodConnections; i++){
                        
                        //does this line exist?
                        if((flockPtr->neighborhoodConnections[i]->bID == theBoid->globalID && flockPtr->neighborhoodConnections[i]->aID == iterator->globalID) || (flockPtr->neighborhoodConnections[i]->aID == theBoid->globalID && flockPtr->neighborhoodConnections[i]->bID == iterator->globalID)){
                            lineAlreadyExists = 1;
                            break;
                        }

                    }
                    
                    //If this is a new line, create it and add it to the neighborhoodConnections array
                    if(!lineAlreadyExists){
                        NeighborLinePtr newLine = InitNeighborhoodLine(flockPtr, theBoid, iterator);
                        if(!newLine){
                            post("ERROR: Failed to allocate a line");
                            continue;
                        }
                        
                        //add the new line to the array
                        flockPtr->neighborhoodConnections[flockPtr->sizeOfNeighborhoodConnections] = newLine;
                        flockPtr->sizeOfNeighborhoodConnections++;
                    }
                    
                }
                
                neighborsCount++;
                
            }
            /*else if(dist == 0.0 && iterator->flockID == theBoid->flockID){
                //When the boid sees itself, it should draw lines to all the boids remaining in the LL if they are close enough
                startAddingBoidLines = 1;
            }
             */
            
            iterator = iterator->nextBoid; //move to next boid
        }
    }
    
    //normalize the velocities
    NormalizeVelocity(matchNeighborVel);
    NormalizeVelocity(separationNeighborVel);
    
    //update the center point as an average of theBoid's neighbors
    if(neighborsCount > 0){ //get the average position of all boids in the flock
        flockPtr->tempCenterPt[x] = (double)	(totalH / neighborsCount);
        flockPtr->tempCenterPt[y] = (double)	(totalV / neighborsCount);
        flockPtr->tempCenterPt[z] = (double)	(totalD / neighborsCount);
    }else{ //only boid in flock, its position is the center point
        flockPtr->tempCenterPt[x] = theBoid->oldPos[x];
        flockPtr->tempCenterPt[y] = theBoid->oldPos[y];
        flockPtr->tempCenterPt[z] = theBoid->oldPos[z];
    }
}


/*!
    @brief Computes a normalized direction vector from a boid to a seek point
    @param flockPtr A pointer to the flocks object
    @param theBoid The boid object that the direction vector is calculated for
    @param seekPt The point that the boid is seeking
    @param seekDir The calculated direction is stored here
 */
void SeekPoint(t_jit_boids3d *flockPtr, BoidPtr theBoid, double *seekPt, double* seekDir)
{
    seekDir[x] = seekPt[x] - theBoid->oldPos[x];
    seekDir[y] = seekPt[y] - theBoid->oldPos[y];
    seekDir[z] = seekPt[z] - theBoid->oldPos[z];
    NormalizeVelocity(seekDir);
}



/*!
    @brief Computes a normalized seek direction from a boid towards every attractor
    @param flockPtr A pointer to the flocks object
    @param theBoid The boid object that the direction vector is calculated for
    @param seekDir The calculated direction is stored here
 */
void SeekAttractors(t_jit_boids3d *flockPtr, BoidPtr theBoid, double* seekDir)
{
    AttractorPtr iterator = flockPtr->attractorLL;
    
    //iterate thru and sum up the direction to all attractors
    while(iterator){
        
        double dist = sqrt(DistSqrToPt(iterator->loc, theBoid->oldPos));
        
        //ensure that the boid is in range of the attractor and it is allowed to feel attraction to this attractor
        if(dist < iterator->attractorRadius && (iterator->onlyAttractedFlockID == -1 || iterator->onlyAttractedFlockID == theBoid->flockID)){
            seekDir[x] += iterator->loc[x]-theBoid->oldPos[x];
            seekDir[y] += iterator->loc[y]-theBoid->oldPos[y];
            seekDir[z] += iterator->loc[z]-theBoid->oldPos[z];
        }
        
        iterator = iterator->nextAttractor;
    }
    
    NormalizeVelocity(seekDir);
}


/*!
    @brief Bounces boids back from the walls of the simulation if necessary
    @param flockPtr A point to the flocks object
    @param theBoid The boid object that is being bounced back from the walls
    @param wallVel The resulting direction after bouncing off the wall
 */
void AvoidWalls(t_jit_boids3d *flockPtr, BoidPtr theBoid, double *wallVel)
{
    double		testPoint[3];
    
    /* calculate test point in front of the nose of the boid */
    /* distance depends on the boid's speed and the avoid edge constant */
    testPoint[x] = theBoid->oldPos[x] + theBoid->newDir[x] * (theBoid->speed * (flockPtr->speed[theBoid->flockID] / 100.0));// * flockPtr->edgedist[flockPtr->boid[theBoid].flockID];
    testPoint[y] = theBoid->oldPos[y] + theBoid->newDir[y] * (theBoid->speed * (flockPtr->speed[theBoid->flockID] / 100.0));// * flockPtr->edgedist[flockPtr->boid[theBoid].flockID];
    testPoint[z] = theBoid->oldPos[z] + theBoid->newDir[z] * (theBoid->speed * (flockPtr->speed[theBoid->flockID] / 100.0));// * flockPtr->edgedist[flockPtr->boid[theBoid].flockID];
    
    
    /* if test point is out of the left (right) side of flockPtr->flyrect, */
    /* return a positive (negative) horizontal velocity component */
    if (testPoint[x] < flockPtr->flyrect[left]*kFlyRectScalingFactor)
        wallVel[x] = ABS(wallVel[x]);
    else if (testPoint[x] > flockPtr->flyrect[right]*kFlyRectScalingFactor)
        wallVel[x] = - ABS(wallVel[x]);
    
    /* same with top and bottom */
    if (testPoint[y] > flockPtr->flyrect[top]*kFlyRectScalingFactor)
        wallVel[y] = - ABS(wallVel[y]);
    else if (testPoint[y] < flockPtr->flyrect[bottom]*kFlyRectScalingFactor)
        wallVel[y] = ABS(wallVel[y]);
    
    /* same with front and back*/
    if (testPoint[z] > flockPtr->flyrect[front]*kFlyRectScalingFactor)
        wallVel[z] = - ABS(wallVel[z]);
    else if (testPoint[z] < flockPtr->flyrect[back]*kFlyRectScalingFactor)
        wallVel[z] = ABS(wallVel[z]);
}


/*!
    @brief Determines if a neighbor boid is in front of a given boid
    @param theBoid A boid
    @param neighbor A different boid - calculates if this boid is in front of theBoid
    @return 0 if neighbor is not in front, 1 if it is
 */
char InFront(BoidPtr theBoid, BoidPtr neighbor)
{
    float	grad, intercept;
    char result;
    
    /* we do this on 2 planes, xy, yz. if one returns false then we know its behind. a.sier/jasch 08/2005
     
     Find the gradient and y-intercept of a line passing through theBoid's oldPos
     perpendicular to its direction of motion.  Another boid is in front of theBoid
     if it is to the right or left of this linedepending on whether theBoid is moving
     right or left.  However, if theBoid is travelling vertically then just compare
     their vertical coordinates.
     
     */
    // xy plane
    
    // if theBoid is not travelling vertically...
    if (theBoid->oldDir[x] != 0) {
        // calculate gradient of a line _perpendicular_ to its direction (hence the minus)
        grad = -theBoid->oldDir[y] / theBoid->oldDir[x];
        
        // calculate where this line hits the y axis (from y = mx + c)
        intercept = theBoid->oldPos[y] - (grad * theBoid->oldPos[x]);
        
        /* compare the horizontal position of the neighbor boid with */
        /* the point on the line that has its vertical coordinate */
        if (neighbor->oldPos[x] >= ((neighbor->oldPos[y] - intercept) / grad)) {
            /* return true if the first boid's horizontal movement is +ve */
            result = (theBoid->oldDir[x] > 0);
            
            if (result==0) return 0;
            else goto next;
            
        } else {
            /* return true if the first boid's horizontal movement is +ve */
            result = (theBoid->oldDir[x] < 0);
            if (result==0) return 0;
            else goto next;
        }
    }
    /* else theBoid is travelling vertically, so just compare vertical coordinates */
    else if (theBoid->oldDir[y] > 0) {
        result = (neighbor->oldPos[y] > theBoid->oldPos[y]);
        if (result==0){
            return 0;
        }else{
            goto next;
        }
    }else{
        result = (neighbor->oldPos[y] < theBoid->oldPos[y]);
        if (result==0){
            return 0;
        } else {
            goto next;
        }
    }
next:
    
    // yz plane
    
    // if theBoid is not travelling vertically...
    if (theBoid->oldDir[y] != 0) {
        // calculate gradient of a line _perpendicular_ to its direction (hence the minus)
        grad = -theBoid->oldDir[z] / theBoid->oldDir[y];
        
        // calculate where this line hits the y axis (from y = mx + c)
        intercept = theBoid->oldPos[z] - (grad * theBoid->oldPos[y]);
        
        // compare the horizontal position of the neighbor boid with
        // the point on the line that has its vertical coordinate
        if (neighbor->oldPos[y] >= ((neighbor->oldPos[z] - intercept) / grad)) {
            // return true if the first boid's horizontal movement is +ve
            result = (theBoid->oldDir[y] > 0);
            if (result==0){
                return 0;
            }else{
                goto next2;
            }
        } else {
            // return true if the first boid's horizontal movement is +ve
            result = (theBoid->oldDir[y] < 0);
            if (result==0){
                return 0;
            }else{
                goto next2;
            }
        }
    }
    // else theBoid is travelling vertically, so just compare vertical coordinates
    else if (theBoid->oldDir[z] > 0) {
        result = (neighbor->oldPos[z] > theBoid->oldPos[z]);
        if (result==0){
            return 0;
        }else{
            goto next2;
        }
    }else{
        result = (neighbor->oldPos[z] < theBoid->oldPos[z]);
        if (result==0){
            return 0;
        }else{
            goto next2;
        }
    }
next2:
    return 1;
}

void NormalizeVelocity(double *direction)
{
    float	hypot;
    
    hypot = jit_math_sqrt(direction[x] * direction[x] + direction[y] * direction[y] + direction[z] * direction[z] );
    
    if (hypot != 0.0) {
        direction[x] = direction[x] / hypot;
        direction[y] = direction[y] / hypot;
        direction[z] = direction[z] / hypot;
    }
}


/*!
    @brief Returns a random integer in the specified range
 */
double RandomInt(double minRange, double maxRange)
{
    double	t, result;
    
    t = (double)(jit_rand() & 0x0000FFFF)/(double)(0x0000FFFF);
    
    result = (t * (maxRange - minRange)) + minRange;
    return(result);
}


/*!
    @brief Returns the squared distance between 2 points
 */
double DistSqrToPt(double *firstPoint, double *secondPoint)
{
    double	a, b,c;
    a = firstPoint[x] - secondPoint[x];
    b = firstPoint[y] - secondPoint[y];
    c = firstPoint[z] - secondPoint[z];
    return(a * a + b * b + c * c);
}



//
//
//      MARK: Initialization and Free methods
//
//


/*!
    @brief Initializes everything related to the flock object
    @param flockPtr a point to the flock object
 */
void InitFlock(t_jit_boids3d *flockPtr)
{
    //General initialization
    flockPtr->number            = kNumBoids*MAX_FLOCKS;	//added init for jitter object
    flockPtr->neighbors			= kNumNeighbors;
    
    //boundary initialization
    flockPtr->flyrect[top]		= kFlyRectTop;
    flockPtr->flyrect[left]		= kFlyRectLeft;
    flockPtr->flyrect[bottom]	= kFlyRectBottom;
    flockPtr->flyrect[right]	= kFlyRectRight;
    flockPtr->flyrect[front]	= kFlyRectFront;
    flockPtr->flyrect[back]		= kFlyRectBack;
    
    //attractor initialization
    flockPtr->attractorLL = NULL;
    flockPtr->numAttractors = 0;
    
    //other initialization
    flockPtr->sizeOfNeighborhoodConnections = 0;
    flockPtr->drawingNeighbors = 0;
    flockPtr->newBoidID = 0;
    
    //set the initial birth location to the origin
    flockPtr->birthLoc[x] = 0.0;
    flockPtr->birthLoc[y] = 0.0;
    flockPtr->birthLoc[z] = 0.0;
    
    //Flock specific initialization
    for(int i=0; i<MAX_FLOCKS; i++){
        
        if (kNumBoids == 0) { //to avoid crashing problem
            continue;
        }
        
        //create the linked list
        BoidPtr newLL = InitLL(flockPtr, kNumBoids, i);
        
        ///!!! error checking here breaks the external
        if(!newLL){
            return;
        }
        
        flockPtr->flockLL[i] = newLL; //add LL to the array
        
        //default values, will be changed when the parameters in the max patch are banged
        flockPtr->minspeed[i]			= kMinSpeed;
        flockPtr->maxspeed[i]			= kMaxSpeed;
        flockPtr->center[i]             = kCenterWeight;
        flockPtr->attract[i]			= kAttractWeight;
        flockPtr->match[i]				= kMatchWeight;
        flockPtr->sepwt[i]				= kSepWeight;
        flockPtr->sepdist[i]			= kSepDist;
        flockPtr->speed[i]				= kDefaultSpeed;
        flockPtr->inertia[i]			= kInertiaFactor;
        flockPtr->accel[i]              = kAccelFactor;
        flockPtr->neighborRadius[i]     = kNRadius;
        flockPtr->boidCount[i] = 0;
    }
}


/*!
    @brief Calculates and returns the total number of boids across all flocks
 */
int CalcNumBoids(t_jit_boids3d *flockPtr)
{
    int boidsCounter = 0;
    for (int i=0; i<MAX_FLOCKS; i++){
        boidsCounter += flockPtr->boidCount[i];
    }
    return boidsCounter;
}


/*! 
    @brief Initializes a linked list of boids
    @param flockPtr a pointer to the flock object
    @param numBoids The number of boids that will be added to this flock
    @param flockID Which flock the boids of this linked list belong to
    @return A pointer to boid that is the head of the linked list
 */
BoidPtr InitLL(t_jit_boids3d *flockPtr, long numBoids, int flockID)
{
    BoidPtr head = NULL;
    for(int i=0; i < numBoids; i++){
        
        BoidPtr theBoid = InitBoid(flockPtr); //make a new boid
        if(!theBoid){
            post("ERROR: failed to malloc a boid");
            return NULL;
        }
        
        //Add the boid to the LL or make it the head (if nothing has already been added)
        if (head == NULL){
            head = theBoid;
            theBoid->nextBoid = NULL;
        }else{
            theBoid->nextBoid = head;
            head = theBoid;
        }
        
        //update number of boids in the flock
        flockPtr->boidCount[flockID]++;
    }
    
    return head;
}

/*!
    @brief Creates a NeighborLine object to connect 2 boids
    @param flockPtr a pointer to the flock object
    @param theBoid A boid that is one endpoint of the line
    @param theOtherBoid A boid that is the other endpoint of the line
    @param id The flock ID of both boids (both boids WILL belong to same flock)
    @return A pointer to the new NeighborLine object
 */
NeighborLinePtr InitNeighborhoodLine(t_jit_boids3d *flockPtr, BoidPtr theBoid, BoidPtr theOtherBoid)
{
    //allocate memory for the line
    struct NeighborLine * theLine = (struct NeighborLine *)malloc(sizeof(struct NeighborLine));
    if(!theLine){
        return NULL;
    }
    
    //Initialize components of the struct
    theLine->boidA[x] = theBoid->newPos[x];
    theLine->boidA[y] = theBoid->newPos[y];
    theLine->boidA[z] = theBoid->newPos[z];
    theLine->aID = theBoid->globalID;
    
    theLine->boidB[x] = theOtherBoid->newPos[x];
    theLine->boidB[y] = theOtherBoid->newPos[y];
    theLine->boidB[z] = theOtherBoid->newPos[z];
    theLine->bID = theOtherBoid->globalID;
    
    theLine->flockID[0] = theBoid->flockID;
    theLine->flockID[1] = theOtherBoid->flockID;
    
    return theLine;
}


/*!
    @brief Initializes a boid object
    @param flockPtr a pointer to the flock object
    @return a pointer to the created boid
 */
BoidPtr InitBoid(t_jit_boids3d *flockPtr)
{
    //allocate memory for the boid
    struct Boid * theBoid = (struct Boid *)malloc(sizeof(struct Boid));
    
    if(!theBoid){
        return NULL;
    }
    
    theBoid->age = 0; //set age to 0
    
    //assign the boid a unique ID
    theBoid->globalID = flockPtr->newBoidID;
    flockPtr->newBoidID++;
    
    //initialize struct variables
    theBoid->oldPos[x] = 0.0;
    theBoid->oldPos[y] = 0.0;
    theBoid->oldPos[z] = 0.0;
    
    theBoid->newPos[x] = 0.0;
    theBoid->newPos[y] = 0.0;
    theBoid->newPos[z] = 0.0;
    
    theBoid->oldDir[x] = 0.0;
    theBoid->oldDir[y] = 0.0;
    theBoid->oldDir[z] = 0.0;
    
    theBoid->newDir[x] = 0.0;
    theBoid->newDir[y] = 0.0;
    theBoid->newDir[z] = 0.0;
    
    theBoid->speed = 0.0;
    
    //    theBoid->newPos[x] = theBoid->oldPos[x] = (kFlyRectScalingFactor*RandomInt(flockPtr->flyrect[right],flockPtr->flyrect[left]));		// set random location within flyrect
    //    theBoid->newPos[y] = theBoid->oldPos[y] = (kFlyRectScalingFactor*RandomInt(flockPtr->flyrect[bottom], flockPtr->flyrect[top]));
    //    theBoid->newPos[z] = theBoid->oldPos[z] = (kFlyRectScalingFactor*RandomInt(flockPtr->flyrect[back], flockPtr->flyrect[front]));
    
    //set the boids position to be birthLoc
    theBoid->newPos[x] = theBoid->oldPos[x] = flockPtr->birthLoc[x];
    theBoid->newPos[y] = theBoid->oldPos[y] = flockPtr->birthLoc[y];
    theBoid->newPos[z] = theBoid->oldPos[z] = flockPtr->birthLoc[z];
    
    double rndAngle = RandomInt(0, 360) * flockPtr->d2r;		// set velocity from random angle
    theBoid->newDir[x] = jit_math_sin(rndAngle);
    theBoid->newDir[y] = jit_math_cos(rndAngle);
    theBoid->newDir[z] = (jit_math_cos(rndAngle) + jit_math_sin(rndAngle)) * 0.5;
    theBoid->speed = (kMaxSpeed + kMinSpeed) * 0.5;
    
    for(int j=0; j<kMaxNeighbors;j++) {
        theBoid->neighbor[j] = 0;
        theBoid->neighborDistSqr[j] = 0.0;
    }
    
    return theBoid;
}


/*!
    @brief Initializes an Attractor object
    @param flockPtr a pointer to the flock object
    @return a pointer to the created Attractor object
 */
AttractorPtr InitAttractor(t_jit_boids3d *flockPtr)
{
    //initialize memory for the attractor
    struct Attractor * theAttractor = (struct Attractor *)malloc(sizeof(struct Attractor));
    if(!theAttractor){
        return NULL;
    }
    
    //TODO: make the attractor at a specific point (not always the origin)
    theAttractor->loc[0] = 0.0;
    theAttractor->loc[1] = 0.0;
    theAttractor->loc[2] = 0.0;
    
    theAttractor->onlyAttractedFlockID = -1;
    theAttractor->attractorRadius = 0.0;
    
    return theAttractor;
}


/*!
    @brief Does initialization for the jit_boids3d object
 */
t_jit_boids3d *jit_boids3d_new(void)
{
    t_jit_boids3d *flockPtr;
    
    if ((flockPtr=(t_jit_boids3d *)jit_object_alloc(_jit_boids3d_class))) {
        
        flockPtr->flyRectCount		= 6;
        flockPtr->mode	 			= 0;
        flockPtr->allowNeighborsFromDiffFlock = 0;
        
        //init boids params
        InitFlock(flockPtr);
        
        flockPtr->d2r = 3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068/180.0;
        flockPtr->r2d = 180.0/3.141592653589793238462643383279502884197169399375105820974944592307816406286208998628034825342117068;
    } else {
        flockPtr = NULL;
    }
    return flockPtr;
}


/*!
    @brief Frees memory for all of the flocks in the simulation
 */
void freeFlocks(t_jit_boids3d *flockPtr)
{
    for(int i=0; i<MAX_FLOCKS; i++){ //we're clearing each flock
        
        if(flockPtr->flockLL[i] == NULL){ //ensure that this flock is populated
            continue;
        }
        
        BoidPtr iterator = flockPtr->flockLL[i];
        BoidPtr deletor = iterator;
        
        do{ //traverse the LL and free the memory for each boid
            iterator = iterator->nextBoid;
            
            free(deletor);
            deletor = iterator;
            
        }while (iterator);
        
        //mark the head null
        flockPtr->flockLL[i] = NULL;
        
    }
}
