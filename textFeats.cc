/**
 * Tool that extracts text feature vectors for a given Page XMLs or images
 *
 * @version $Revision$$Date::             $
 * @copyright Copyright (c) 2016 to the present, Mauricio Villegas <mauvilsa@upv.es>
 */

/*
 @todo Better parallelization: i.e. threads also read pages
 @todo Fix output list of extractions
*/

/*** Includes *****************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <libconfig.h++>
#include <regex>
#include <chrono>

#include "TextFeatExtractor.h"
#include "PageXML.h"
#include "log.h"

using namespace std;
using namespace libconfig;

/*** Definitions **************************************************************/
static char tool[] = "textFeats";
static char revnum[] = "$Revision$";
static char revdate[] = "$Date$";

FILE *logfile = NULL;
int verbosity = 1;

char   gb_default_outdir[] = ".";
char   gb_default_feaext[] = "fea";
char   gb_default_imgext[] = "png";
char   gb_default_xpath[] = "//_:TextRegion/_:TextLine/_:Coords[@points and @points!=\"0,0 0,0\"]";

char  *gb_cfgfile = NULL;
bool   gb_overwrite = false;
char  *gb_outdir = gb_default_outdir;
bool   gb_featlist = false;
char  *gb_feaext = gb_default_feaext;
char  *gb_imgext = gb_default_imgext;
char  *gb_xpath = gb_default_xpath;
bool   gb_saveclean = false;
bool   gb_savefeaimg = false;
bool   gb_savexml = false;
bool   gb_fpoints = true;
int    gb_numrand = 0;
bool   gb_firstrand = false;

int                  gb_numthreads = 1;
int                 *gb_threadnum = NULL;
pthread_t           *gb_threads = NULL;
pthread_mutex_t      gb_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned             gb_next_image = 0;
vector<NamedImage>   gb_images = vector<NamedImage>();
int                  gb_numextract = 0;
bool                 gb_isxml;
bool                 gb_failure = false;

TextFeatExtractor   *gb_extractor = NULL;
PageXML             *gb_page = NULL;

enum {
  OPTION_HELP      = 'h',
  OPTION_VERSION   = 'v',
  OPTION_VERBOSE   = 'V',
  OPTION_THREADS   = 'T',
  OPTION_CFGFILE   = 'C',
  OPTION_OVERWRITE = 'O',
  OPTION_OUTDIR    = 'o',
  OPTION_FEATLIST  = 'L',
  OPTION_FEATYPE   = 256,
  OPTION_FEAEXT         ,
  OPTION_IMGEXT         ,
  OPTION_XPATH          ,
  OPTION_SAVECLEAN      ,
  OPTION_SAVEFEAIMG     ,
  OPTION_SAVEXML        ,
  OPTION_FPOINTS        ,
  OPTION_NUMRAND        ,
  OPTION_FIRSTRAND
};

static char gb_short_options[] = "hvVT:C:Oo:L";

static struct option gb_long_options[] = {
    { "help",        no_argument,       NULL, OPTION_HELP },
    { "version",     no_argument,       NULL, OPTION_VERSION },
    { "verbose",     optional_argument, NULL, OPTION_VERBOSE },
    { "threads",     required_argument, NULL, OPTION_THREADS },
    { "cfg",         required_argument, NULL, OPTION_CFGFILE },
    { "overwrite",   optional_argument, NULL, OPTION_OVERWRITE },
    { "outdir",      required_argument, NULL, OPTION_OUTDIR },
    { "featlist",    optional_argument, NULL, OPTION_FEATLIST },
    { "feaext",      required_argument, NULL, OPTION_FEAEXT },
    { "imgext",      required_argument, NULL, OPTION_IMGEXT },
    { "xpath",       required_argument, NULL, OPTION_XPATH },
    { "saveclean",   optional_argument, NULL, OPTION_SAVECLEAN },
    { "savefeaimg",  optional_argument, NULL, OPTION_SAVEFEAIMG },
    { "savexml",     optional_argument, NULL, OPTION_SAVEXML },
    { "fpoints",     optional_argument, NULL, OPTION_FPOINTS },
    { "rand",        required_argument, NULL, OPTION_NUMRAND },
    { "firstrand",   optional_argument, NULL, OPTION_FIRSTRAND },
    { 0, 0, 0, 0 }
  };

/*** Functions ****************************************************************/
void print_usage( FILE *file ) {
  print_svn_rev( file );
  fprintf( file, "Description: Extracts text feature vectors for given Page XMLs or images\n" );
  fprintf( file, "Usage 1: %s [options] <page1.xml> [<page2.xml> ...]\n", tool );
  fprintf( file, "Usage 2: %s [options] <textimage1> [<textimage2> ...]\n", tool );
  fprintf( file, "Options:\n" );
  fprintf( file, " -h --help                      Print this usage information and exit\n" );
  fprintf( file, " -v --version                   Print tool version and exit\n" );
  fprintf( file, " -V --verbose[=(-|+|level)]     Verbosity level (def.=%d)\n", verbosity );
  fprintf( file, " -T --threads NUM               Number of parallel threads (def.=%d)\n", gb_numthreads );
  fprintf( file, " -C --cfg CFGFILE               Configuration file for TextFeatExtractor and PageXML (def.=none)\n" );
  fprintf( file, " -O --overwrite[=(true|false)]  Overwrite existing files (def.=%s)\n", strbool(gb_overwrite) );
  fprintf( file, " -o --outdir OUTDIR             Output directory (def.=%s)\n", gb_outdir );
  fprintf( file, " -L --featlist[=(true|false)]   Print list of extracted features to stdout (def.=%s)\n", strbool(gb_featlist) );
  fprintf( file, "    --feaext EXT                Output features file extension (def.=%s)\n", gb_feaext );
  fprintf( file, "    --imgext EXT                Output images file extension (def.=%s)\n", gb_imgext );
  fprintf( file, "    --xpath XPATH               xpath for selecting text samples (def.=%s)\n", gb_xpath );
  fprintf( file, "    --saveclean[=(true|false)]  Save clean images (def.=%s)\n", strbool(gb_saveclean) );
  fprintf( file, "    --savefeaimg[=(true|false)] Save features images (def.=%s)\n", strbool(gb_savefeaimg) );
  fprintf( file, "    --savexml[=(true|false)]    Save XML with extraction information (def.=%s)\n", strbool(gb_savexml) );
  fprintf( file, "    --fpoints[=(true|false)]    Store feature contours in points attribute (def.=%s)\n", strbool(gb_fpoints) );
  fprintf( file, "    --rand NUM                  Number of random perturbed extractions per sample (def.=%d)\n", gb_numrand );
  fprintf( file, "    --firstrand[=(true|false)]  Whether the first extraction is perturbed (def.=%s)\n", strbool(gb_firstrand) );
}

inline bool file_exists( const char* fname ) {
  return ( access( fname, F_OK ) != -1 );
}

inline float time_diff( chrono::high_resolution_clock::time_point tm ) {
  return 0.001*chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now()-tm).count();
}

inline bool parse_bool( char* str ) {
  if( str ) {
    if( !strcasecmp("true",str) || !strcasecmp("yes",str) )
      return true;
    return false;
  }
  return true;
}

/*** Program ******************************************************************/
int main( int argc, char *argv[] ) {
  logfile = stderr;

  /// Parse input arguments ///
  int n,m;
  while( ( n = getopt_long(argc,argv,gb_short_options,gb_long_options,&m) ) != -1 )
    switch( n ) {
      case OPTION_OVERWRITE:
        gb_overwrite = parse_bool(optarg);
        break;
      case OPTION_OUTDIR:
        gb_outdir = optarg;
        break;
      case OPTION_FEATLIST:
        gb_featlist = parse_bool(optarg);
        break;
      case OPTION_FEAEXT:
        gb_feaext = optarg;
        break;
      case OPTION_IMGEXT:
        gb_imgext = optarg;
        break;
      case OPTION_XPATH:
        gb_xpath = optarg;
        break;
      case OPTION_SAVECLEAN:
        gb_saveclean = parse_bool(optarg);
        break;
      case OPTION_SAVEFEAIMG:
        gb_savefeaimg = parse_bool(optarg);
        break;
      case OPTION_SAVEXML:
        gb_savexml = parse_bool(optarg);
        break;
      case OPTION_FPOINTS:
        gb_fpoints = parse_bool(optarg);
        break;
      case OPTION_NUMRAND:
        gb_numrand = atoi(optarg);
        break;
      case OPTION_FIRSTRAND:
        gb_firstrand = parse_bool(optarg);
        break;
      case OPTION_CFGFILE:
        gb_cfgfile = optarg;
        break;
      case OPTION_THREADS:
        gb_numthreads = atoi(optarg);
        break;
      case OPTION_VERBOSE:
        if( ! optarg )
          verbosity ++;
        else if( optarg[0] == '+' )
          verbosity ++;
        else if( optarg[0] == '-' )
          verbosity --;
        else
          verbosity = atoi( optarg );
        verbosity = verbosity < 0 ? 0 : verbosity;
        break;
      case OPTION_HELP:
        print_usage( logfile );
        return SUCCESS;
      case OPTION_VERSION:
        print_svn_rev( logfile );
        return SUCCESS;
      default:
        die( "error: incorrect input argument: %s", argv[optind-1] );
    }

  if( optind >= argc )
    die( "error: expected at least one Page XML or line image file" );

  /// Print configuration ///
  logger( 3, "config: overwrite files: %s", strbool(gb_overwrite) );
  logger( 3, "config: output directory: %s", gb_outdir );
  logger( 3, "config: print list of extracted features: %s", strbool(gb_featlist) );
  logger( 3, "config: output features file extension: %s", gb_feaext );
  logger( 3, "config: output images file extension: %s", gb_imgext );
  logger( 3, "config: text coords selector xpath: %s", gb_xpath );
  logger( 3, "config: save clean images: %s", strbool(gb_saveclean) );
  logger( 3, "config: save XML with extraction information: %s", strbool(gb_savexml) );
  logger( 3, "config: store feature contours in points attribute: %s", strbool(gb_fpoints) );

  /// Load configuration ///
  Config cfg;
  if( gb_cfgfile != NULL )
    try {
      cfg.readFile(gb_cfgfile);
    }
    catch( const ParseException &pex ) {
      die( "error: parse error in config file at line %d: %s", pex.getLine(), pex.getError() );
    }

  /// Create feature extractor object ///
  TextFeatExtractor extractor(cfg);
  //extractor.loadConf( cfg );
  if( verbosity >= 3 )
    extractor.printConf( logfile );
  gb_extractor = &extractor;

  /// Create page loader object ///
  PageXML page(cfg);
  //page.loadConf( cfg );
  if( verbosity >= 3 )
    page.printConf( logfile );
  gb_page = &page;

  /// Auxiliary stuff ///
  regex reXml(".+\\.xml",regex_constants::icase);
  regex reBase1(".*/([^/]+)\\.[^.]+");
  regex reBase2("(.+)\\.[^.]+");
  chrono::high_resolution_clock::time_point tm;
  chrono::high_resolution_clock::time_point tottm = chrono::high_resolution_clock::now();

  gb_threads = new pthread_t[gb_numthreads];
  gb_threadnum = new int[gb_numthreads];
  for( int n=0; n<gb_numthreads; n++ )
    gb_threadnum[n] = n;

  /// Loop for processing input files ///
  for( int n=optind; n<argc; n++ ) {
    logger( 1, "processing file %d: %s", n-optind+1, argv[n] );

    gb_isxml = regex_match(argv[n],reXml);
    // @todo Allow "-" for Page XML from stdin

    /// Read Page XML file ///
    if( gb_isxml ) {
      tm = chrono::high_resolution_clock::now();
      page.loadXml( argv[n] );
      gb_images = page.crop( gb_xpath );
      logger( 2, "page read and line cropping time: %.0f ms", time_diff(tm) );
    }

    /// Or read a set of input images ///
    else {
      gb_images.clear();
      smatch base_match;

      for( int m=n; m<min(argc,n+100); m++ ) {
        string argvn = string(argv[m]);
        string linename =
          ( regex_match(argvn,base_match,reBase1) || regex_match(argvn,base_match,reBase2) ) ?
          base_match[1].str() :
          argvn ;

        Magick::Image lineimg;
        lineimg.read(argv[m]);

        NamedImage namedline = { linename, linename, lineimg };
        gb_images.push_back(namedline);
      }
    }

    gb_next_image = 0;

    /// Start threads and wait for them to finish ///
    void* extractionThread( void* _num ); // Defined below
    for( int n=gb_numthreads-1; n>=0; n-- )
      pthread_create( &gb_threads[n], NULL, extractionThread, (void*)&gb_threadnum[n] );
    for( int n=gb_numthreads-1; n>=0; n-- )
      pthread_join( gb_threads[n], NULL );

    //if( gb_featlist && ! gb_failure )
    //  for( int k=0; k<(int)gb_images.size(); k++ )
    //    printf("%s\n",gb_images[k].name.c_str());

    /// Save Page XML with feature extraction information ///
    if( gb_isxml && gb_savexml ) {
      string outfile = string(gb_outdir)+'/'+page.getBase()+".xml";
      if( ! gb_overwrite && file_exists(outfile.c_str()) ) {
        logger( 0, "error: aborted write to existing file: %s", outfile.c_str() );
        gb_failure = true;
      }
      page.write( outfile.c_str() );
    }
  }

  logger( 2, "extracted features for %d images", gb_numextract );
  logger( 2, "total time: %.0f ms", time_diff(tottm) );

  /// Release resources ///
  xmlCleanupParser();
  pthread_mutex_destroy(&gb_mutex);
  //pthread_exit(NULL); // hangs here, why?

  return gb_failure ? FAILURE : SUCCESS ;
}

/**
 * Function for parallel extraction of features with pthread.
 */
void* extractionThread( void* _num ) {
  int thread = *((int*)_num);

  while( true ) {

    /// Thread sage selection of image to process ///
    pthread_mutex_lock( &gb_mutex );
    if( gb_next_image >= gb_images.size() ) {
      pthread_mutex_unlock( &gb_mutex );
      break;
    }
    int image_num = gb_next_image;
    gb_next_image++;
    gb_numextract++;
    pthread_mutex_unlock( &gb_mutex );

    /// Perform extraction ///
    chrono::high_resolution_clock::time_point tm = chrono::high_resolution_clock::now();
    logger( 4, "extracting: %s (thread %d)", gb_images[image_num].name.c_str(), thread );

    float slope, slant;
    vector<cv::Point2f> fpgram;
    vector<cv::Point> fcontour;

    Magick::Image prepimage = gb_images[image_num].image;

    /// Clean and enhance image ///
    gb_extractor->preprocess( prepimage, &fcontour );
    if( gb_saveclean ) {
      string outfile = string(gb_outdir)+'/'+gb_images[image_num].name+"_clean."+gb_imgext;
      if( ! gb_overwrite && file_exists(outfile.c_str()) ) {
        logger( 0, "error: aborted write to existing file: %s", outfile.c_str() );
        gb_failure = true;
      }
      prepimage.write( outfile.c_str() );
    }

    /// Estimate slope and slant (sets them to 0 if disabled) ///
    gb_extractor->estimateAngles( prepimage, &slope, &slant );

    /// Get x-height ///
    int xheight = 0;
    if( gb_isxml )
      xheight = gb_page->getXheight( gb_images[image_num].id.c_str() );
    // @todo x-height estimation from image
    //xheight = extractor.estimateXheight( prepimage );
    //logger( 0, "xheight=%d", xheight );

    /// Loop for random perturbation extractions ///
    int R = gb_numrand == 0 ? 1 : gb_numrand ;
    for( int r=0; r<R; r++ ) {
      bool randpert = r > 0 || ( r== 0 && gb_firstrand ) ;
      string outname = string(gb_outdir)+'/'+gb_images[image_num].name;
      outname += gb_numrand ? "_"+to_string(r) : "" ;

      Magick::Image featimage = prepimage;

      /// Redo preprocessing for random perturbation ///
      if( randpert ) {
        featimage = gb_images[image_num].image;
        gb_extractor->preprocess( featimage, NULL, randpert );
      }

      /// Extract features ///
      cv::Mat feats = gb_extractor->extractFeats( featimage, slope, slant, xheight, &fpgram, randpert );

      /// Write features to file ///
      char *feaext = gb_extractor->isImageFormat() ? gb_imgext : gb_feaext ;
      if( ! gb_overwrite && file_exists((outname+"."+feaext).c_str()) ) {
        logger( 0, "error: aborted write to existing file: %s", (outname+"."+feaext).c_str() );
        gb_failure = true;
      }
      gb_extractor->write( feats, (outname+"."+feaext).c_str() );

      /// Write features image to file ///
      if( gb_savefeaimg && ! gb_extractor->isImageFormat() ) {
        if( ! gb_overwrite && file_exists((outname+"_fea."+gb_imgext).c_str()) ) {
          logger( 0, "error: aborted write to existing file: %s", (outname+"_fea."+gb_imgext).c_str() );
          gb_failure = true;
        }
        featimage.write( (outname+"_fea."+gb_imgext).c_str() );
      }
    }

    logger( 3, "feature extraction time: %.0f ms", time_diff(tm) );

    /// Add extraction information to Page XML ///
    if( gb_isxml && gb_savexml ) {
      string xpath = string("//*[@id='")+gb_images[image_num].id+"']/_:Coords";
      char sslope[16], sslant[16];
      int m  = sprintf( sslope, "%g", slope );
      m += sprintf( sslant, "%g", slant );
      if( slope != 0.0 )
        gb_page->setAttr( xpath.c_str(), "slope", sslope );
      if( slant != 0.0 )
        gb_page->setAttr( xpath.c_str(), "slant", sslant );
      if( fpgram.size() > 0 )
        gb_page->setAttr( xpath.c_str(), "fpgram", gb_page->pointsToString(fpgram).c_str() );
      if( fcontour.size() > 0 )
        gb_page->setAttr( xpath.c_str(), gb_fpoints ? "points" : "fcontour", gb_page->pointsToString(fcontour).c_str() );
    }

    /// Extracted features list ///
    //if( gb_featlist ) {
    //  pthread_mutex_lock( &gb_mutex );
    //  printf("%s\n",gb_images[image_num].name.c_str());
    //  pthread_mutex_unlock( &gb_mutex );
    //}
  }

  pthread_exit((void*)0);
  //return NULL;
}
