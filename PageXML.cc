/**
 * Class for input, output and processing of Page XML files and referenced image.
 *
 * @version $Revision::       $$Date::             $
 * @copyright Copyright (c) 2016 to the present, Mauricio Villegas <mauvilsa@upv.es>
 */

#include "PageXML.h"

#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <regex>

#include <opencv2/opencv.hpp>
#include <libxml/xpathInternals.h>

using namespace std;
using namespace Magick;
using namespace libconfig;

const char* PageXML::settingNames[] = {
  "indent",
  "pagens",
  "grayimg",
  "extended_names"
};

char default_pagens[] = "http://schema.primaresearch.org/PAGE/gts/pagecontent/2013-07-15";

Color transparent("rgba(0,0,0,0)");
Color opaque("rgba(0,0,0,100%)");
regex reXheight(".*x-height: *([0-9.]+) *px;.*");
regex reRotation(".*readingOrientation: *([0-9.]+) *;.*");
regex reDirection(".*readingDirection: *([lrt]t[rlb]) *;.*");


/////////////////////////
/// Resources release ///
/////////////////////////

/**
 * Releases all reserved resources of PageXML instance.
 */
void PageXML::release() {
  if( xml == NULL )
    return;

  pageimg = Image();
  if( xml != NULL )
    xmlFreeDoc(xml);
  xml = NULL;
  if( context != NULL )
    xmlXPathFreeContext(context);
  context = NULL;
  if( xmldir != NULL )
    free(xmldir);
  xmldir = NULL;
  if( imgpath != NULL )
    free(imgpath);
  imgpath = NULL;
  if( imgbase != NULL )
    free(imgbase);
  imgbase = NULL;
}

/**
 * PageXML object destructor.
 */
PageXML::~PageXML() {
  release();
}

////////////////////
/// Constructors ///
////////////////////

/**
 * PageXML constructor that receives a libconfig Config object.
 *
 * @param config  A libconfig Config object.
 */
PageXML::PageXML( const Config& config ) {
  loadConf(config);
  if( pagens == NULL )
    pagens = default_pagens;
}

/**
 * PageXML constructor that receives a configuration file name.
 *
 * @param cfgfile  Configuration file to use.
 */
PageXML::PageXML( const char* cfgfile ) {
  if( cfgfile != NULL ) {
    Config config;
    config.readFile(cfgfile);
    loadConf(config);
  }
  if( pagens == NULL )
    pagens = default_pagens;
}

//////////////
/// Output ///
//////////////

/**
 * Writes the current state of the XML to a file using utf-8 encoding.
 *
 * @param fname  File name of where the XML file will be written.
 * @return       Number of bytes written.
 */
int PageXML::write( const char* fname ) {
  return xmlSaveFormatFileEnc( fname, xml, "utf-8", indent );
}

/**
 * Gets the base name of the Page file derived from the image file name.
 *
 * @return  String containing the image base name.
 */
char* PageXML::getBase() {
  return imgbase;
}

/////////////////////
/// Configuration ///
/////////////////////

/**
 * Gets the enum value for a configuration setting name, or -1 if unknown.
 *
 * @param format  String containing setting name.
 * @return        Enum format value.
 */
inline static int parsePageSetting( const char* setting ) {
  int settings = sizeof(PageXML::settingNames) / sizeof(PageXML::settingNames[0]);
  for( int n=0; n<settings; n++ )
    if( ! strcmp(PageXML::settingNames[n],setting) )
      return n;
  return -1;
}

/**
 * Applies configuration options to the PageXML instance.
 *
 * @param config  A libconfig Config object.
 */
void PageXML::loadConf( const Config& config ) {
  if( ! config.exists("PageXML") )
    return;

  const Setting& pagecfg = config.getRoot()["PageXML"];

  int numsettings = pagecfg.getLength();
  for( int i = 0; i < numsettings; i++ ) {
    const Setting& setting = pagecfg[i];
    //printf("PageXML: setting=%s enum=%d\n",setting.getName(),parsePageSetting(setting.getName()));
    switch( parsePageSetting(setting.getName()) ) {
      case PAGEXML_SETTING_INDENT:
        indent = (bool)setting;
        break;
      case PAGEXML_SETTING_PAGENS:
        if( pagens != NULL && pagens != default_pagens )
          free(pagens);
        pagens = strdup(setting.c_str());
        break;
      case PAGEXML_SETTING_GRAYIMG:
        grayimg = (bool)setting;
        break;
      case PAGEXML_SETTING_EXTENDED_NAMES:
        extended_names = (bool)setting;
        break;
      default:
        throw invalid_argument( string("PageXML: unexpected configuration property: ") + setting.getName() );
    }
  }
}

/**
 * Prints the current configuration.
 *
 * @param file  File to print to.
 */
void PageXML::printConf( FILE* file ) {
  fprintf( file, "PageXML: {\n" );
  fprintf( file, "  indent = %s;\n", indent ? "true" : "false" );
  fprintf( file, "  pagens = \"%s\";\n", pagens );
  fprintf( file, "  grayimg = %s;\n", grayimg ? "true" : "false" );
  fprintf( file, "  extended_names = %s;\n", extended_names ? "true" : "false" );
  fprintf( file, "}\n" );
}

///////////////
/// Loaders ///
///////////////

/**
 * Loads a Page XML from a file.
 *
 * @param fname  File name of the XML file to read.
 */
void PageXML::loadXml( const char* fname ) {
  release();

  if( strrchr(fname,'/') != NULL )
    xmldir = strndup(fname,strrchr(fname,'/')-fname);
  FILE *file;
  if( (file=fopen(fname,"rb")) == NULL )
    throw runtime_error( string("PageXML: loadXml: unable to open file: ") + fname );
  loadXml( fileno(file) );
  fclose(file);
}

/**
 * Loads a Page XML from an input stream.
 *
 * @param fnum  File number from where to read the XML file.
 */
void PageXML::loadXml( int fnum ) {
  release();

  xmlKeepBlanksDefault(0);
  xml = xmlReadFd( fnum, NULL, NULL, XML_PARSE_NONET );

  context = xmlXPathNewContext(xml);
  if( context == NULL )
    throw runtime_error( "PageXML: loadXml: unable create xpath context" );
  if( xmlXPathRegisterNs( context, (xmlChar*)"_", (xmlChar*)pagens ) != 0 )
    throw runtime_error( "PageXML: loadXml: unable to register namespace" );
  rootnode = context->node;
  rpagens = xmlSearchNsByHref(xml,xmlDocGetRootElement(xml),(xmlChar*)pagens);

  vector<xmlNodePtr> elem_page = select( "//_:Page" );
  if( elem_page.size() == 0 )
    throw runtime_error( "PageXML: loadXml: unable to find page element" );

  /// Get page size ///
  char* uwidth = (char*)xmlGetProp( elem_page[0], (xmlChar*)"imageWidth" );
  char* uheight = (char*)xmlGetProp( elem_page[0], (xmlChar*)"imageHeight" );
  if( uwidth == NULL || uheight == NULL )
    throw runtime_error( "PageXML: loadXml: problems retrieving page size from xml" );
  width = atoi(uwidth);
  height = atoi(uheight);
  free(uwidth);
  free(uheight);

  /// Get image path ///
  imgpath = (char*)xmlGetProp( elem_page[0], (xmlChar*)"imageFilename" );
  if( imgpath ==NULL )
    throw runtime_error( "PageXML: loadXml: problems retrieving image file from xml" );

  char* p = strrchr(imgpath,'/') == NULL ? imgpath : strrchr(imgpath,'/')+1;
  if( strrchr(p,'.') == NULL )
    throw runtime_error( string("PageXML: loadXml: expected image file name to have an extension: ")+p );
  imgbase = strndup(p,strrchr(p,'.')-p);
  for( char *p=imgbase; *p!='\0'; p++ )
    if( *p == ' ' /*|| *p == '[' || *p == ']' || *p == '(' || *p == ')'*/ )
      *p = '_';

  if( xmldir == NULL )
    xmldir = strdup(".");
}

/**
 * Loads an image for the Page XML.
 *
 * @param fname  File name of the image to read.
 */
void PageXML::loadImage( const char* fname ) {
  string aux;
  if( fname == NULL )
    fname = imgpath[0] == '/' ? imgpath : (aux=string(xmldir)+'/'+imgpath).c_str();

  try {
    pageimg.read(fname);
  } 
  catch( exception& e ) {
    throw runtime_error( string("PageXML: loadImage: ") + e.what() );
  } 

  if( grayimg ) {
    if( pageimg.matte() && pageimg.type() != GrayscaleMatteType )
      pageimg.type( GrayscaleMatteType );
    else if( ! pageimg.matte() && pageimg.type() != GrayscaleType )
      pageimg.type( GrayscaleType );
  }

  if( width != pageimg.columns() || height != pageimg.rows() )
    throw runtime_error( string("PageXML: loadImage: discrepancy between image and xml page size: ") + fname );
}

///////////////////////
/// Page processing ///
///////////////////////

/**
 * Converts an cv vector of points to a Magick++ list of Coordinates.
 *
 * @param points   Vector of points.
 * @return         List of coordinates.
 */
list<Coordinate> PageXML::cvToMagick( const vector<cv::Point2f>& points ) {
  list<Coordinate> mpoints;
  for( int n=0; n<(int)points.size(); n++ ) {
    mpoints.push_back( Coordinate( points[n].x, points[n].y ) );
  }
  return mpoints;
}

/**
 * Parses a string of pairs of coordinates (x1,y1 [x2,y2 ...]) into an array.
 *
 * @param spoints  String containing coordinate pairs.
 * @param points   Array of (x,y) coordinates.
 */
void PageXML::stringToPoints( const char* spoints, vector<cv::Point2f>& points ) {
  points.empty();

  int n = 0;
  char *p = (char*)spoints-1;
  while( true ) {
    p++;
    double x, y;
    if( sscanf( p, "%lf,%lf", &x, &y ) != 2 )
      break;
    points.push_back(cv::Point2f(x,y));
    n++;
    if( (p = strchr(p,' ')) == NULL )
      break;
  }
}

/**
 * Parses a string of pairs of coordinates (x1,y1 [x2,y2 ...]) into an array.
 *
 * @param spoints  String containing coordinate pairs.
 * @param points   Array of (x,y) coordinates.
 */
void PageXML::stringToPoints( string spoints, vector<cv::Point2f>& points ) {
  stringToPoints( spoints.c_str(), points );
}

/**
 * Gets the minimum and maximum coordinate values for an array of points.
 *
 * @param points     The vector of points to find the limits.
 * @param xmin       Minimum x value.
 * @param xmax       Maximum x value.
 * @param ymin       Minimum y value.
 * @param ymax       Maximum y value.
 */
void PageXML::pointsLimits( vector<cv::Point2f>& points, double& xmin, double& xmax, double& ymin, double& ymax ) {
  if( points.size() == 0 )
    return;

  xmin = xmax = points[0].x;
  ymin = ymax = points[0].y;

  for( auto&& point : points ) {
    if( xmin > point.x ) xmin = point.x;
    if( xmax < point.x ) xmax = point.x;
    if( ymin > point.y ) ymin = point.y;
    if( ymax < point.y ) ymax = point.y;
  }

  return;
}

/**
 * Generates a vector of 4 points that define the bounding box for a given vector of points.
 *
 * @param points     The vector of points to find the limits.
 * @param xmin       Minimum x value.
 * @param xmax       Maximum x value.
 * @param ymin       Minimum y value.
 * @param ymax       Maximum y value.
 */
void PageXML::pointsBBox( vector<cv::Point2f>& points, vector<cv::Point2f>& bbox ) {
  bbox.empty();
  if( points.size() == 0 )
    return;

  double xmin;
  double xmax;
  double ymin;
  double ymax;

  pointsLimits( points, xmin, xmax, ymin, ymax );
  bbox.push_back( cv::Point2f(xmin,ymin) );
  bbox.push_back( cv::Point2f(xmax,ymin) );
  bbox.push_back( cv::Point2f(xmax,ymax) );
  bbox.push_back( cv::Point2f(xmin,ymax) );

  return;
}

/**
 * Determines whether a vector of points defines a bounding box.
 *
 * @param points   The vector of points to find the limits.
 * @return         True if bounding box, otherwise false.
 */
bool PageXML::isBBox( const vector<cv::Point2f>& points ) {
  if( points.size() == 4 &&
      points[0].x == points[3].x &&
      points[0].y == points[1].y &&
      points[1].x == points[2].x &&
      points[2].y == points[3].y )
    return true;
  return false;
}

/**
 * Cronvers a vector of points to a string in format "x1,y1 x2,y2 ...".
 *
 * @param points   Vector of points.
 * @param rounded  Whether to round values.
 * @return         String representation of the points.
 */
string PageXML::pointsToString( vector<cv::Point2f> points, bool rounded ) {
  char val[64];
  string str("");
  for( size_t n=0; n<points.size(); n++ ) {
    sprintf( val, rounded ? "%.0f,%.0f" : "%g,%g", points[n].x, points[n].y );
    str += ( n == 0 ? "" : " " ) + string(val);
  }
  return str;
}

/**
 * Cronvers a vector of points to a string in format "x1,y1 x2,y2 ...".
 *
 * @param points  Vector of points.
 * @return        String representation of the points.
 */
string PageXML::pointsToString( vector<cv::Point> points ) {
  char val[32];
  string str("");
  for( size_t n=0; n<points.size(); n++ ) {
    sprintf( val, "%d,%d", points[n].x, points[n].y );
    str += ( n == 0 ? "" : " " ) + string(val);
  }
  return str;
}

/**
 * Selects nodes given an xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Vector of matched nodes.
 */
vector<xmlNodePtr> PageXML::select( const char* xpath, xmlNodePtr basenode ) {
  vector<xmlNodePtr> matched;

#define __REUSE_CONTEXT__

  xmlXPathContextPtr ncontext = context;
  if( basenode != NULL ) {
#ifndef __REUSE_CONTEXT__
    ncontext = xmlXPathNewContext(basenode->doc);
    if( ncontext == NULL )
      throw runtime_error( "PageXML: select: unable create xpath context" );
    if( xmlXPathRegisterNs( ncontext, (xmlChar*)"_", (xmlChar*)pagens ) != 0 )
      throw runtime_error( "PageXML: select: unable to register namespace" );
#endif
    ncontext->node = basenode;
  }

  xmlXPathObjectPtr xsel = xmlXPathEvalExpression( (xmlChar*)xpath, ncontext );
#ifdef __REUSE_CONTEXT__
  if( basenode != NULL )
    ncontext->node = rootnode;
#else
  if( ncontext != context )
    xmlXPathFreeContext(ncontext);
#endif

  if( xsel == NULL )
    throw runtime_error( string("PageXML: xpath expression failed: ") + xpath );
  else {
    if( ! xmlXPathNodeSetIsEmpty(xsel->nodesetval) )
      for( int n=0; n<xsel->nodesetval->nodeNr; n++ )
        matched.push_back( xsel->nodesetval->nodeTab[n] );
    xmlXPathFreeObject(xsel);
  }

  return matched;
}

/**
 * Selects nodes given an xpath.
 *
 * @param xpath  Selector expression.
 * @param node   XML node for context, set to NULL for root node.
 * @return       Vector of matched nodes.
 */
vector<xmlNodePtr> PageXML::select( string xpath, xmlNodePtr node ) {
  return select( xpath.c_str(), node );
}

/**
 * Crops images using its Coords polygon, regions outside the polygon are set to transparent.
 *
 * @param xpath  Selector for polygons to crop.
 * @return       An std::vector containing (id,name,image) triplets of the cropped images.
 */
vector<NamedImage> PageXML::crop( const char* xpath ) {
  vector<NamedImage> images;

  vector<xmlNodePtr> elems_coords = select( xpath );
  if( elems_coords.size() == 0 )
    return images;

  if( pageimg.columns() == 0 )
    loadImage();

  for( int n=0; n<(int)elems_coords.size(); n++ ) {
    xmlNodePtr node = elems_coords[n];

    if( xmlStrcmp( node->name, (const xmlChar*)"Coords") )
      throw runtime_error( string("PageXML: expected xpath to match only Coords elements: match=") + to_string(n+1) + " xpath=" + xpath );

    /// Get parent node id ///
    string sampid;
    if( ! getAttr( node->parent, "id", sampid ) )
      throw runtime_error( string("PageXML: expected parent element to include id attribute: match=") + to_string(n+1) + " xpath=" + xpath );

    /// Construct sample name ///
    string sampname = string(".") + sampid;
    if( extended_names )
      if( ! xmlStrcmp( node->parent->name, (const xmlChar*)"TextLine") ||
          ! xmlStrcmp( node->parent->name, (const xmlChar*)"Word") ) {
        string id2;
        getAttr( node->parent->parent, "id", id2 );
        sampname = string(".") + id2 + sampname;
        if( ! xmlStrcmp( node->parent->name, (const xmlChar*)"Word") ) {
          string id3;
          getAttr( node->parent->parent->parent, "id", id3 );
          sampname = string(".") + id3 + sampname;
        }
      }
    sampname = string(imgbase) + sampname;

    /// Get coords points ///
    string spoints;
    if( ! getAttr( node, "points", spoints ) )
      throw runtime_error( string("PageXML: expected a points attribute in Coords element: id=") + sampid );
    vector<cv::Point2f> coords;
    stringToPoints( spoints, coords );

    /// Get crop window parameters ///
    double xmin, xmax, ymin, ymax;
    pointsLimits( coords, xmin, xmax, ymin, ymax );
    size_t cropW = (size_t)(ceil(xmax)-floor(xmin)+1);
    size_t cropH = (size_t)(ceil(ymax)-floor(ymin)+1);
    int cropX = (int)floor(xmin);
    int cropY = (int)floor(ymin);

    /// Crop image ///
    Image cropimg = pageimg;
    cropimg.crop( Geometry(cropW,cropH,cropX,cropY) );

    if( ! isBBox( coords ) ) {
      /// Subtract crop window offset ///
      for( auto&& coord : coords ) {
        coord.x -= cropX;
        coord.y -= cropY;
      }

      /// Add transparency layer ///
      list<Drawable> drawList;
      drawList.push_back(DrawableStrokeColor(opaque));
      drawList.push_back(DrawableFillColor(opaque));
      drawList.push_back(DrawableStrokeAntialias(false));
      drawList.push_back(DrawablePolygon(cvToMagick(coords)));
      Image mask( Geometry(cropW,cropH), transparent );
      mask.draw(drawList);
      cropimg.draw( DrawableCompositeImage(0,0,0,0,mask,CopyOpacityCompositeOp) );
    }

    /// Append crop and related data to list ///
    NamedImage namedimage = {
      sampid,
      sampname,
      getRotation(node->parent),
      getReadingDirection(node->parent),
      cropimg };

    images.push_back(namedimage);
  }

  return images;
}

/**
 * Gets an attribute value from an xml node.
 *
 * @param node   XML node.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
bool PageXML::getAttr( const xmlNodePtr node, const char* name, string& value ) {
  if( node == NULL )
    return false;
 
  xmlChar* attr = xmlGetProp( node, (xmlChar*)name );
  value = string((char*)attr);
  xmlFree(attr);

  return true;
}

/**
 * Gets an attribute value for a given xpath.
 *
 * @param xpath  Selector for the element to get the attribute.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
bool PageXML::getAttr( const char* xpath, const char* name, string& value ) {
  vector<xmlNodePtr> xsel = select( xpath );
  if( xsel.size() == 0 )
    return false;
 
  return getAttr( xsel[0], name, value );
}

/**
 * Gets an attribute value for a given xpath.
 *
 * @param xpath  Selector for the element to get the attribute.
 * @param name   Attribute name.
 * @param value  String to set the value.
 * @return       True if attribute found, otherwise false.
*/
bool PageXML::getAttr( const string xpath, const string name, string& value ) {
  vector<xmlNodePtr> xsel = select( xpath.c_str() );
  if( xsel.size() == 0 )
    return false;
 
  return getAttr( xsel[0], name.c_str(), value );
}

/**
 * Adds or modifies (if already exists) an attribute for a given list of nodes.
 *
 * @param nodes  Vector of nodes to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( vector<xmlNodePtr> nodes, const char* name, const char* value ) {
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlAttrPtr attr = xmlHasProp( nodes[n], (xmlChar*)name ) ?
      xmlSetProp( nodes[n], (xmlChar*)name, (xmlChar*)value ) :
      xmlNewProp( nodes[n], (xmlChar*)name, (xmlChar*)value ) ;
    if( ! attr )
      throw runtime_error( string("PageXML: setAttr: problems setting attribute: name=") + name );
  }

  return (int)nodes.size();
}

/**
 * Adds or modifies (if already exists) an attribute for a given node.
 *
 * @param node   Node to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( xmlNodePtr node, const char* name, const char* value ) {
  return setAttr( vector<xmlNodePtr>{node}, name, value );
}

/**
 * Adds or modifies (if already exists) an attribute for a given xpath.
 *
 * @param xpath  Selector for the element(s) to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( const char* xpath, const char* name, const char* value ) {
  return setAttr( select(xpath), name, value );
}

/**
 * Adds or modifies (if already exists) an attribute for a given xpath.
 *
 * @param xpath  Selector for the element(s) to set the attribute.
 * @param name   Attribute name.
 * @param value  Attribute value.
 * @return       Number of elements modified.
 */
int PageXML::setAttr( const string xpath, const string name, const string value ) {
  return setAttr( select(xpath.c_str()), name.c_str(), value.c_str() );
}

/**
 * Creates a new element and adds it relative to a given node.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param node   Reference element for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::addElem( const char* name, const char* id, const xmlNodePtr node, PAGEXML_INSERT itype, bool checkid ) {
  xmlNodePtr elem = xmlNewNode( rpagens, (xmlChar*)name );
  if( ! elem )
    throw runtime_error( string("PageXML: addElem: problems creating new element: name=") + name );
  if( id != NULL ) {
    if( checkid ) {
      vector<xmlNodePtr> idsel = select( (string("//*[@id='")+id+"']").c_str() );
      if( idsel.size() > 0 )
        throw runtime_error( string("PageXML: addElem: id already exists: id=") + id );
    }
    xmlNewProp( elem, (xmlChar*)"id", (xmlChar*)id );
  }

  switch( itype ) {
    case PAGEXML_INSERT_CHILD:
      elem = xmlAddChild(node,elem);
      break;
    case PAGEXML_INSERT_NEXTSIB:
      elem = xmlAddNextSibling(node,elem);
      break;
    case PAGEXML_INSERT_PREVSIB:
      elem = xmlAddPrevSibling(node,elem);
      break;
  }

  return elem;
}

/**
 * Creates a new element and adds it relative to a given xpath.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param xpath  Selector for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::addElem( const char* name, const char* id, const char* xpath, PAGEXML_INSERT itype, bool checkid ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML: addElem: unmatched target: xpath=") + xpath );

  return addElem( name, id, target[0], itype, checkid );
}

/**
 * Creates a new element and adds it relative to a given xpath.
 *
 * @param name   Name of element to create.
 * @param id     ID attribute for element.
 * @param xpath  Selector for insertion.
 * @param itype  Type of insertion.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::addElem( const string name, const string id, const string xpath, PAGEXML_INSERT itype, bool checkid ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML: addElem: unmatched target: xpath=") + xpath );

  return addElem( name.c_str(), id.c_str(), target[0], itype, checkid );
}

/**
 * Removes the elements given in a vector.
 *
 * @param nodes  Vector of elements.
 * @return       Number of elements removed.
 */
int PageXML::rmElems( const vector<xmlNodePtr>& nodes ) {
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    xmlUnlinkNode(nodes[n]);
    xmlFreeNode(nodes[n]);
  }

  return (int)nodes.size();
}

/**
 * Remove the elements that match a given xpath.
 *
 * @param xpath  Selector for elements to remove.
 * @param node   Base node for element selection.
 * @return       Number of elements removed.
 */
int PageXML::rmElems( const char* xpath, xmlNodePtr basenode ) {
  return rmElems( select( xpath, basenode ) );
}

/**
 * Remove the elements that match a given xpath.
 *
 * @param xpath    Selector for elements to remove.
 * @param basenode Base node for element selection.
 * @return         Number of elements removed.
 */
int PageXML::rmElems( const string xpath, xmlNodePtr basenode ) {
  return rmElems( select( xpath.c_str(), basenode ) );
}

/**
 * Retrieves the rotation angle for a given TextLine or TextRegion node.
 *
 * @param elem   Node of the TextLine or TextRegion element.
 * @return       The rotation angle in degrees, 0 if unset.
 */
float PageXML::getRotation( const xmlNodePtr elem ) {
  float rotation = 0;
  if( elem == NULL )
    return rotation;

  xmlNodePtr node = elem;

  /// If TextLine try to get rotation from custom attribute ///
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextLine") ) {
    if( ! xmlHasProp( node, (xmlChar*)"custom" ) )
      node = node->parent;
    else {
      xmlChar* attr = xmlGetProp( node, (xmlChar*)"custom" );
      cmatch base_match;
      if( regex_match((char*)attr,base_match,reRotation) )
        rotation = stof(base_match[1].str());
      else
        node = node->parent;
      xmlFree(attr);
    }
  }
  /// Otherwise try to get rotation from readingOrientation attribute ///
  if( xmlHasProp( node, (xmlChar*)"readingOrientation" ) ) {
    xmlChar* attr = xmlGetProp( node, (xmlChar*)"readingOrientation" );
    rotation = stof((char*)attr);
    xmlFree(attr);
  }

  return rotation;
}

/**
 * Retrieves the reading direction for a given TextLine or TextRegion node.
 *
 * @param elem   Node of the TextLine or TextRegion element.
 * @return       The reading direction, DIRECTION_LTR if unset.
 */
int PageXML::getReadingDirection( const xmlNodePtr elem ) {
  int direction = DIRECTION_LTR;
  if( elem == NULL )
    return direction;

  xmlNodePtr node = elem;

  /// If TextLine try to get direction from custom attribute ///
  if( ! xmlStrcmp( node->name, (const xmlChar*)"TextLine") ) {
    if( ! xmlHasProp( node, (xmlChar*)"custom" ) )
      node = node->parent;
    else {
      xmlChar* attr = xmlGetProp( node, (xmlChar*)"custom" );
      cmatch base_match;
      if( regex_match((char*)attr,base_match,reDirection) ) {
        if( ! strcmp(base_match[1].str().c_str(),"ltr") )
          direction = DIRECTION_LTR;
        else if( ! strcmp(base_match[1].str().c_str(),"rtl") )
          direction = DIRECTION_RTL;
        else if( ! strcmp(base_match[1].str().c_str(),"ttb") )
          direction = DIRECTION_TTB;
        else if( ! strcmp(base_match[1].str().c_str(),"btt") )
          direction = DIRECTION_BTT;
      }
      else
        node = node->parent;
      xmlFree(attr);
    }
  }
  /// Otherwise try to get direction from readingDirection attribute ///
  if( xmlHasProp( node, (xmlChar*)"readingDirection" ) ) {
    char* attr = (char*)xmlGetProp( node, (xmlChar*)"readingDirection" );
    if( ! strcmp(attr,"left-to-right") )
      direction = DIRECTION_LTR;
    else if( ! strcmp(attr,"right-to-left") )
      direction = DIRECTION_RTL;
    else if( ! strcmp(attr,"top-to-bottom") )
      direction = DIRECTION_TTB;
    else if( ! strcmp(attr,"bottom-to-top") )
      direction = DIRECTION_BTT;
    xmlFree(attr);
  }

  return direction;
}

/**
 * Retrieves the x-height for a given TextLine node.
 *
 * @param node   Node of the TextLine element.
 * @return       x-height>0 on success, -1 if unset.
 */
float PageXML::getXheight( const xmlNodePtr node ) {
  float xheight = -1;

  if( node != NULL &&
      xmlHasProp( node, (xmlChar*)"custom" ) ) {
    xmlChar* custom = xmlGetProp( node, (xmlChar*)"custom" );
    cmatch base_match;
    if( regex_match((char*)custom,base_match,reXheight) )
      xheight = stof(base_match[1].str());
    xmlFree(custom);
  }

  return xheight;
}

/**
 * Retrieves the x-height for a given TextLine id.
 *
 * @param id     Identifier of the TextLine.
 * @return       x-height>0 on success, -1 if unset.
 */
float PageXML::getXheight( const char* id ) {
  vector<xmlNodePtr> elem = select( string("//*[@id='")+id+"']" );
  return getXheight( elem.size() == 0 ? NULL : elem[0] );
}

/**
 * Retrieves the features parallelogram for a given TextLine id.
 *
 * @param id     Identifier of the TextLine.
 * @return       Parallelogram, empty list if unset.
 */
bool PageXML::getFpgram( const xmlNodePtr node, vector<cv::Point2f>& fpgram ) {
  if( node == NULL )
    return false;

  vector<xmlNodePtr> coords = select( "_:Coords[@fpgram]", node );
  if( coords.size() == 0 )
    return false;

  string sfpgram;
  if( ! getAttr( coords[0], "fpgram", sfpgram ) )
    return false;

  stringToPoints( sfpgram.c_str(), fpgram );

  return true;
}

/**
 * Retrieves and parses the Coords/@points for a given base node.
 *
 * @param node   Base node.
 * @return       Pointer to the points vector, NULL if unset.
 */
bool PageXML::getPoints( const xmlNodePtr node, vector<cv::Point2f>& points ) {
  if( node == NULL )
    return false;

  vector<xmlNodePtr> coords = select( "_:Coords[@points]", node );
  if( coords.size() == 0 )
    return false;

  string spoints;
  if( ! getAttr( coords[0], "points", spoints ) )
    return false;

  stringToPoints( spoints.c_str(), points );

  return true;
}

/**
 * Adds or modifies (if already exists) the TextEquiv for a given node.
 *
 * @param node   The node of element to set the TextEquiv.
 * @param text   The text string.
 * @param conf   The confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setTextEquiv( xmlNodePtr node, const char* text, const char* conf ) {
  rmElems( select( "_:TextEquiv", node ) );

  xmlNodePtr textequiv = addElem( "TextEquiv", NULL, node );

  xmlNodePtr unicode = xmlNewTextChild( textequiv, NULL, (xmlChar*)"Unicode", (xmlChar*)text );
  if( ! unicode )
    throw runtime_error( "PageXML: setTextEquiv: problems setting TextEquiv" );

  if( conf != NULL )
    if( ! xmlNewProp( textequiv, (xmlChar*)"conf", (xmlChar*)conf ) )
      throw runtime_error( "PageXML: setTextEquiv: problems setting conf attribute" );

  return textequiv;
}

/**
 * Adds or modifies (if already exists) the TextEquiv for a given xpath.
 *
 * @param node   The node of element to set the TextEquiv.
 * @param text   The text string.
 * @param conf   The confidence value, NULL for no confidence.
 * @return       Pointer to created element.
 */
xmlNodePtr PageXML::setTextEquiv( const char* xpath, const char* text, const char* conf ) {
  vector<xmlNodePtr> target = select( xpath );
  if( target.size() == 0 )
    throw runtime_error( string("PageXML: setTextEquiv: unmatched target: xpath=") + xpath );

  return setTextEquiv( target[0], text, conf );
}

/**
 * Verifies that all IDs in page are unique.
 */
bool PageXML::uniqueIDs() {
  string id;
  bool unique = true;
  map<string,bool> seen;

  vector<xmlNodePtr> nodes = select( "//*[@id]" );
  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    getAttr( nodes[n], "id", id );
    if( seen.find(id) != seen.end() && seen[id] ) {
      fprintf( stderr, "PageXML: uniqueIDs: duplicate id: %s\n", id.c_str() );
      seen[id] = unique = false;
    }
    else
      seen[id] = true;
  }

  return unique;
}

/**
 * Simplifies IDs by removing imgbase prefixes and replaces invalid characters with _.
 *
 * @return       Number of IDs simplified.
 */
int PageXML::simplifyIDs() {
  int simplified = 0;

  regex reTrim("^[^a-zA-Z]*");
  regex reInvalid("[^a-zA-Z0-9_-]");
  string sampbase(imgbase);

  vector<xmlNodePtr> nodes = select( "//*[@id][local-name()='TextLine' or local-name()='TextRegion']" );

  for( int n=(int)nodes.size()-1; n>=0; n-- ) {
    char* id = (char*)xmlGetProp( nodes[n], (xmlChar*)"id" );
    string sampid(id);
    if( sampid.size() > sampbase.size() &&
        ! sampid.compare(0,sampbase.size(),sampbase) ) {
      sampid.erase( 0, sampbase.size() );
      sampid = regex_replace(sampid,reTrim,"");
      if( sampid.size() > 0 ) {
        sampid = regex_replace(sampid,reInvalid,"_");
        setAttr( nodes[n], "orig-id", id );
        setAttr( nodes[n], "id", sampid.c_str() );
        simplified++;
      }
    }
    free(id);
  }

  return simplified;
}
