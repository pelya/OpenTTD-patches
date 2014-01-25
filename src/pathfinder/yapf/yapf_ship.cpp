/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file yapf_ship.cpp Implementation of YAPF for ships. */

#include "../../stdafx.h"
#include "../../ship.h"

#include "yapf.hpp"
#include "yapf_node_ship.hpp"

/** Node Follower module of YAPF for ships */
template <class Types>
class CYapfFollowShipT
{
public:
	typedef typename Types::Tpf Tpf;                     ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::Astar::Node Node;            ///< this will be our node type
	typedef typename Node::Key Key;                      ///< key to hash tables

protected:
	/** to access inherited path finder */
	inline Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	/**
	 * Called by YAPF to move from the given node to the next tile. For each
	 *  reachable trackdir on the new tile creates new node, initializes it
	 *  and adds it to the open list by calling Yapf().AddNewNode(n)
	 */
	inline void PfFollowNode(Node& old_node)
	{
		TrackFollower F(Yapf().GetVehicle());
		if (F.Follow(old_node.GetPos())) {
			Yapf().AddMultipleNodes(&old_node, F);
		}
	}

	/** return debug report character to identify the transportation type */
	inline char TransportTypeChar() const
	{
		return 'w';
	}

	static Trackdir ChooseShipTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs, bool &path_found)
	{
		/* move back to the old tile/trackdir (where ship is coming from) */
		PathPos pos = v->GetPos();
		assert(pos.tile == TILE_ADD(tile, TileOffsByDiagDir(ReverseDiagDir(enterdir))));
		assert(IsValidTrackdir(pos.td));

		/* handle special case - when next tile is destination tile */
		if (tile == v->dest_tile) {
			/* use vehicle's current direction if that's possible, otherwise use first usable one. */
			Trackdir veh_dir = pos.td;
			return ((trackdirs & TrackdirToTrackdirBits(veh_dir)) != 0) ? veh_dir : FindFirstTrackdir(trackdirs);
		}

		/* create pathfinder instance */
		Tpf pf;
		/* set origin and destination nodes */
		pf.SetOrigin(pos);
		pf.SetDestination(v);
		/* find best path */
		path_found = pf.FindPath(v);

		Node *pNode = pf.GetBestNode();
		if (pNode == NULL) return INVALID_TRACKDIR; // path not found

		/* walk through the path back to the origin */
		Node *pPrevNode = NULL;
		while (pNode->m_parent != NULL) {
			pPrevNode = pNode;
			pNode = pNode->m_parent;
		}

		/* return trackdir from the best next node (direct child of origin) */
		Node& best_next_node = *pPrevNode;
		assert(best_next_node.GetPos().tile == tile);
		return best_next_node.GetPos().td;
	}

	/**
	 * Check whether a ship should reverse to reach its destination.
	 * Called when leaving depot.
	 * @param v Ship
	 * @param pos Current position
	 * @return true if the reverse direction is better
	 */
	static bool CheckShipReverse(const Ship *v, const PathPos &pos)
	{
		/* create pathfinder instance */
		Tpf pf;
		/* set origin and destination nodes */
		pf.SetOrigin(pos.tile, TrackdirToTrackdirBits(pos.td) | TrackdirToTrackdirBits(ReverseTrackdir(pos.td)));
		pf.SetDestination(v);
		/* find best path */
		if (!pf.FindPath(v)) return false;

		Node *pNode = pf.GetBestNode();
		if (pNode == NULL) return false;

		/* path was found
		 * walk through the path back to the origin */
		while (pNode->m_parent != NULL) {
			pNode = pNode->m_parent;
		}

		Trackdir best_trackdir = pNode->GetPos().td;
		assert(best_trackdir == pos.td || best_trackdir == ReverseTrackdir(pos.td));
		return best_trackdir != pos.td;
	}
};

/** Cost Provider module of YAPF for ships */
template <class Types>
class CYapfCostShipT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::TrackFollower TrackFollower;
	typedef typename Types::Astar::Node Node;     ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	/** to access inherited path finder */
	Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	/**
	 * Called by YAPF to calculate the cost from the origin to the given node.
	 *  Calculates only the cost of given node, adds it to the parent node cost
	 *  and stores the result into Node::m_cost member
	 */
	inline bool PfCalcCost(Node& n, const TrackFollower *tf)
	{
		/* base tile cost depending on distance */
		int c = IsDiagonalTrackdir(n.GetPos().td) ? YAPF_TILE_LENGTH : YAPF_TILE_CORNER_LENGTH;
		/* additional penalty for curves */
		if (n.GetPos().td != NextTrackdir(n.m_parent->GetPos().td)) {
			/* new trackdir does not match the next one when going straight */
			c += YAPF_TILE_LENGTH;
		}

		/* Skipped tile cost for aqueducts. */
		c += YAPF_TILE_LENGTH * tf->m_tiles_skipped;

		/* Ocean/canal speed penalty. */
		const ShipVehicleInfo *svi = ShipVehInfo(Yapf().GetVehicle()->engine_type);
		byte speed_frac = (GetEffectiveWaterClass(n.GetPos().tile) == WATER_CLASS_SEA) ? svi->ocean_speed_frac : svi->canal_speed_frac;
		if (speed_frac > 0) c += YAPF_TILE_LENGTH * (1 + tf->m_tiles_skipped) * speed_frac / (256 - speed_frac);

		/* apply it */
		n.m_cost = n.m_parent->m_cost + c;
		return true;
	}
};

/** Destination Provider module of YAPF for ships */
template <class Types>
class CYapfDestinationShipT
{
public:
	typedef typename Types::Tpf Tpf;              ///< the pathfinder class (derived from THIS class)
	typedef typename Types::Astar::Node Node;     ///< this will be our node type
	typedef typename Node::Key Key;               ///< key to hash tables

protected:
	Station *m_dest_station;
	TileIndex m_dest_tile;

public:
	/** set the destination tile */
	void SetDestination(const Ship *v)
	{
		if (v->current_order.IsType(OT_GOTO_STATION)) {
			m_dest_station = Station::Get(v->current_order.GetDestination());
			m_dest_tile    = m_dest_station->GetClosestTile(v->tile, STATION_DOCK);
		} else {
			m_dest_station = NULL;
			m_dest_tile    = v->dest_tile;
		}
	}

protected:
	/** to access inherited path finder */
	Tpf& Yapf()
	{
		return *static_cast<Tpf*>(this);
	}

public:
	inline bool PfDetectDestination(const PathPos &pos)
	{
		if (pos.in_wormhole()) return false;

		if (m_dest_station == NULL) return pos.tile == m_dest_tile;

		return m_dest_station->IsDockingTile(pos.tile);
	}

	/** Called by YAPF to detect if node ends in the desired destination */
	inline bool PfDetectDestination(Node& n)
	{
		return PfDetectDestination(n.GetPos());
	}

	/**
	 * Called by YAPF to calculate cost estimate. Calculates distance to the destination
	 *  adds it to the actual cost from origin and stores the sum to the Node::m_estimate
	 */
	inline bool PfCalcEstimate(Node& n)
	{
		static const int dg_dir_to_x_offs[] = {-1, 0, 1, 0};
		static const int dg_dir_to_y_offs[] = {0, 1, 0, -1};
		if (PfDetectDestination(n)) {
			n.m_estimate = n.m_cost;
			return true;
		}

		TileIndex tile = n.GetPos().tile;
		DiagDirection exitdir = TrackdirToExitdir(n.GetPos().td);
		int x1 = 2 * TileX(tile) + dg_dir_to_x_offs[(int)exitdir];
		int y1 = 2 * TileY(tile) + dg_dir_to_y_offs[(int)exitdir];
		int x2 = 2 * TileX(m_dest_tile);
		int y2 = 2 * TileY(m_dest_tile);
		int dx = abs(x1 - x2);
		int dy = abs(y1 - y2);
		int dmin = min(dx, dy);
		int dxy = abs(dx - dy);
		int d = dmin * YAPF_TILE_CORNER_LENGTH + (dxy - 1) * (YAPF_TILE_LENGTH / 2);
		n.m_estimate = n.m_cost + d;
		assert(n.m_estimate >= n.m_parent->m_estimate);
		return true;
	}
};

/**
 * Config struct of YAPF for ships.
 *  Defines all 6 base YAPF modules as classes providing services for CYapfBaseT.
 */
template <class Tpf_, class Ttrack_follower, class TAstar>
struct CYapfShip_TypesT
{
	/** Types - shortcut for this struct type */
	typedef CYapfShip_TypesT<Tpf_, Ttrack_follower, TAstar>  Types;

	/** Tpf - pathfinder type */
	typedef Tpf_                              Tpf;
	/** track follower helper class */
	typedef Ttrack_follower                   TrackFollower;
	/** node list type */
	typedef TAstar                            Astar;
	typedef Ship                              VehicleType;
	/** pathfinder components (modules) */
	typedef CYapfBaseT<Types>                 PfBase;        // base pathfinder class
	typedef CYapfFollowShipT<Types>           PfFollow;      // node follower
	typedef CYapfOriginTileT<Types>           PfOrigin;      // origin provider
	typedef CYapfDestinationShipT<Types>      PfDestination; // destination/distance provider
	typedef CYapfSegmentCostCacheNoneT<Types> PfCache;       // segment cost cache provider
	typedef CYapfCostShipT<Types>             PfCost;        // cost provider
};

/* YAPF type 1 - uses TileIndex/Trackdir as Node key, allows 90-deg turns */
struct CYapfShip1 : CYapfT<CYapfShip_TypesT<CYapfShip1, CFollowTrackWater90  , AstarShipTrackDir> > {};
/* YAPF type 2 - uses TileIndex/DiagDirection as Node key, allows 90-deg turns */
struct CYapfShip2 : CYapfT<CYapfShip_TypesT<CYapfShip2, CFollowTrackWater90  , AstarShipExitDir > > {};
/* YAPF type 3 - uses TileIndex/Trackdir as Node key, forbids 90-deg turns */
struct CYapfShip3 : CYapfT<CYapfShip_TypesT<CYapfShip3, CFollowTrackWaterNo90, AstarShipTrackDir> > {};

/** Ship controller helper - path finder invoker */
Trackdir YapfShipChooseTrack(const Ship *v, TileIndex tile, DiagDirection enterdir, TrackdirBits trackdirs, bool &path_found)
{
	/* default is YAPF type 2 */
	typedef Trackdir (*PfnChooseShipTrack)(const Ship*, TileIndex, DiagDirection, TrackdirBits, bool &path_found);
	PfnChooseShipTrack pfnChooseShipTrack = CYapfShip2::ChooseShipTrack; // default: ExitDir, allow 90-deg

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.forbid_90_deg) {
		pfnChooseShipTrack = &CYapfShip3::ChooseShipTrack; // Trackdir, forbid 90-deg
	} else if (_settings_game.pf.yapf.disable_node_optimization) {
		pfnChooseShipTrack = &CYapfShip1::ChooseShipTrack; // Trackdir, allow 90-deg
	}

	return pfnChooseShipTrack(v, tile, enterdir, trackdirs, path_found);
}

bool YapfShipCheckReverse(const Ship *v)
{
	PathPos pos = v->GetPos();

	typedef bool (*PfnCheckReverseShip)(const Ship*, const PathPos&);
	PfnCheckReverseShip pfnCheckReverseShip = CYapfShip2::CheckShipReverse; // default: ExitDir, allow 90-deg

	/* check if non-default YAPF type needed */
	if (_settings_game.pf.forbid_90_deg) {
		pfnCheckReverseShip = &CYapfShip3::CheckShipReverse; // Trackdir, forbid 90-deg
	} else if (_settings_game.pf.yapf.disable_node_optimization) {
		pfnCheckReverseShip = &CYapfShip1::CheckShipReverse; // Trackdir, allow 90-deg
	}

	bool reverse = pfnCheckReverseShip(v, pos);

	return reverse;
}
