/**
 * Header file defining the configure properties structure
 *
 * @version $Revision::       $$Date::             $
 * @copyright Copyright (c) 2016 to the present, Mauricio Villegas <mauvilsa@upv.es>
 */

#ifndef __CONFIGPROPS_H__
#define __CONFIGPROPS_H__

struct ConfigProps {
  const char *prop;
  bool bval = false;
  int ival = 0;
  double dval = NAN;
  char *sval = NULL;
  ConfigProps( const char *property, bool value ) {
    prop = property;
    bval = value;
  }
  ConfigProps( const char *property, int value ) {
    prop = property;
    ival = value;
  }
  ConfigProps( const char *property, double value ) {
    prop = property;
    dval = value;
  }
  ConfigProps( const char *property, char *value ) {
    prop = property;
    sval = value;
  }
};

#endif
