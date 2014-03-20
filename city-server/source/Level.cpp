#include "Level.h"
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <fstream>
#include <ctime>

using namespace boost;

const float Level::MINIMAL_ATTACK_FACTOR = 0.75;
const float Level::MAXIMAL_ATTACK_FACTOR = 1.25;
const regex Level::LEVEL_FILE_LINE_PATTERN(
	"(0|[1-9]\\d*) (tree|mountain|castle) (0|[1-9]\\d*) (0|[1-9]\\d*)"
);

Level::Level(const std::string& filename) :
	last_player_id(START_PLAYER_ID)
{
	std::srand(std::time(NULL));

	int minimal_x = 0;
	int minimal_y = 0;
	int maximal_x = 0;
	int maximal_y = 0;

	std::ifstream in(filename.c_str());
	while (in) {
		std::string line;
		std::getline(in, line);
		if (line.empty()) {
			continue;
		}

		smatch matches;
		if (regex_match(line, matches, LEVEL_FILE_LINE_PATTERN)) {
			std::string entity_type = matches[2];
			if (
				entity_type == "tree"
				|| entity_type == "mountain"
				|| entity_type == "castle"
			) {
				int x = lexical_cast<int>(matches[3]);
				int y = lexical_cast<int>(matches[4]);

				held_positions.push_back(Position(x, y));

				if (x < minimal_x) {
					minimal_x = x;
				} else if (x > maximal_x) {
					maximal_x = x;
				}
				if (y < minimal_y) {
					minimal_y = y;
				} else if (y > maximal_y) {
					maximal_y = y;
				}
			} else {
				std::cerr
					<< (format(
						"Warning! Invalid entity type \"%s\" in level file.\n"
					) % entity_type).str();
				continue;
			}
		} else {
			std::cerr
				<< (format("Warning! Invalid line \"%s\" in level file.\n")
					% line).str();
		}
	}

	for (int x = minimal_x; x <= maximal_x; ++x) {
		for (int y = minimal_y; y <= maximal_y; ++y) {
			Position position(x, y);
			if (!isPositionHeld(position)) {
				not_held_positions.push_back(position);
			}
		}
	}
}

Level::operator std::string(void) {
	lock_guard<boost::mutex> guard(mutex);

	std::string result;
	std::map<size_t, PlayerSmartPointer>::const_iterator i = players.begin();
	for (; i != players.end(); ++i) {
		result +=
			(format("%u:%u:%i:%i;")
				% i->first
				% i->second->health
				% i->second->position.x
				% i->second->position.y).str();
	}
	result = result.substr(0, result.length() - 1);

	return result;
}

size_t Level::addPlayer(void) {
	lock_guard<boost::mutex> guard(mutex);

	size_t default_health = getDefaultHealth();
	players[last_player_id] = PlayerSmartPointer(new Player(default_health));

	players[last_player_id]->position = getRandomUnholdPosition();
	holdPosition(players[last_player_id]->position);

	return last_player_id++;
}

bool Level::movePlayer(size_t player_id, Direction direction) {
	lock_guard<boost::mutex> guard(mutex);

	if (!players.count(player_id)) {
		throw std::runtime_error("invalid player id");
	}

	Position position = players[player_id]->position;
	switch (direction) {
		case DIRECTION_UP:
			position.y--;
			break;
		case DIRECTION_RIGHT:
			position.x++;
			break;
		case DIRECTION_DOWN:
			position.y++;
			break;
		case DIRECTION_LEFT:
			position.x--;
			break;
	}

	bool can_move = !isPositionHeld(position);
	if (can_move) {
		holdPosition(position);
		unholdPosition(players[player_id]->position);

		players[player_id]->position = position;
	} else {
		size_t enemy_id = getPlayerByPosition(position);
		if (enemy_id) {
			float attack_factor =
				std::rand()
				/ RAND_MAX
				* (MAXIMAL_ATTACK_FACTOR - MINIMAL_ATTACK_FACTOR)
				+ MINIMAL_ATTACK_FACTOR;
			size_t attack_value = static_cast<size_t>(
				std::floor(attack_factor * players[player_id]->health + 0.5f)
			);
			decreasePlayerHealth(enemy_id, attack_value);
		}
	}

	return can_move;
}

void Level::updatePlayerTimestamp(size_t player_id) {
	lock_guard<boost::mutex> guard(mutex);

	if (!players.count(player_id)) {
		throw std::runtime_error("invalid player id");
	}

	players.at(player_id)->timestamp = std::time(NULL);
}

void Level::removeLostPlayers(void) {
	lock_guard<boost::mutex> guard(mutex);

	std::map<size_t, PlayerSmartPointer>::iterator i = players.begin();
	time_t current_timestamp = std::time(NULL);
	while (i != players.end()) {
		if (
			current_timestamp - i->second->timestamp >= MAXIMAL_PLAYER_TIMEOUT
		) {
			unholdPosition(i->second->position);
			players.erase(i++);
		} else {
			++i;
		}
	}
}

bool Level::isPositionHeld(const Position& position) const {
	return
		std::find(held_positions.begin(), held_positions.end(), position)
		!= held_positions.end();
}

void Level::holdPosition(const Position& position) {
	if (!isPositionHeld(position)) {
		held_positions.push_back(position);

		not_held_positions.erase(
			std::remove(
				not_held_positions.begin(),
				not_held_positions.end(),
				position
			),
			not_held_positions.end()
		);
	}
}

void Level::unholdPosition(const Position& position) {
	if (isPositionHeld(position)) {
		held_positions.erase(
			std::remove(
				held_positions.begin(),
				held_positions.end(),
				position
			),
			held_positions.end()
		);

		not_held_positions.push_back(position);
	}
}

Position Level::getRandomUnholdPosition(void) const {
	if (not_held_positions.empty()) {
		throw std::runtime_error("all positions're held");
	}

	return not_held_positions[std::rand() % not_held_positions.size()];
}

size_t Level::getDefaultHealth(void) const {
	if (!players.empty()) {
		size_t health_sum = 0;
		std::map<size_t, PlayerSmartPointer>::const_iterator i =
			players.begin();
		for (; i != players.end(); ++i) {
			health_sum += i->second->health;
		}

		return health_sum / players.size();
	} else {
		return Player::DEFAULT_HEALTH;
	}
}

size_t Level::getPlayerByPosition(const Position& position) const {
	std::map<size_t, PlayerSmartPointer>::const_iterator i = players.begin();
	for (; i != players.end(); ++i) {
		if (i->second->position == position) {
			return i->first;
		}
	}

	return START_PLAYER_ID - 1;
}

void Level::decreasePlayerHealth(size_t player_id, size_t value) {
	if (players[player_id]->health > value) {
		players[player_id]->health -= value;
	} else {
		resetPlayer(player_id);
	}
}

void Level::resetPlayer(size_t player_id) {
	unholdPosition(players[player_id]->position);
	players[player_id]->position = getRandomUnholdPosition();
	holdPosition(players[player_id]->position);

	players[player_id]->health = getDefaultHealth();
}
