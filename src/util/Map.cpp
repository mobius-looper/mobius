/*
 * Copyright (c) 2010 Jeffrey S. Larson  <jeff@circularlabs.com>
 * All rights reserved.
 * See the LICENSE file for the full copyright and license declaration.
 * 
 * ---------------------------------------------------------------------
 * 
 * An approximation of java.util.Map because I can't stand STL.
 * 
 */

#include <map>

#include "Port.h"
#include "Map.h"

struct strCmp {
    bool operator()( const char* s1, const char* s2 ) const {
        return strcmp( s1, s2 ) < 0;
    }
};

// actually it's a tree map
//typedef std::map<const char*, void*> HashMap;
typedef std::map<const char*, void*, strCmp> HashMap;

Map::Map()
{
    init(0);
}

Map::Map(int initialSize)
{
    init(initialSize);
}

Map::~Map()
{
    HashMap* map = (HashMap*)mMap;
    delete map;
}

PRIVATE void Map::init(int size)
{
    // not sure how to specify this
    mMap = (void*)(new HashMap());
}

void Map::put(const char* key, void* value)
{
    HashMap* map = (HashMap*)mMap;
    (*map)[key] = value;
}

void* Map::get(const char* key)
{
    void *value = NULL;
    HashMap* map = (HashMap*)mMap;
    
    // stl map is just fucking unbelievable
    HashMap::const_iterator it = map->find(key);
    if (it != map->end())
      value = it->second;

    return value;
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
