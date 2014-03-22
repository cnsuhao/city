#ifndef CASTLE_H
#define CASTLE_H

#include "Position.h"
#include <boost/shared_ptr.hpp>

class Castle {
public:
	static const size_t START_HEALTH = 0;
	static const size_t DEFAULT_HEALTH = 100;

	Position position;
	time_t timestamp;
	size_t health;
	size_t owner;

	Castle(size_t owner);
};

typedef boost::shared_ptr<Castle> CastleSmartPointer;
#endif
