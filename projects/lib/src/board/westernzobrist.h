/*
    This file is part of Cute Chess.
    Copyright (C) 2008-2018 Cute Chess authors

    Cute Chess is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Cute Chess is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Cute Chess.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef WESTERNZOBRIST_H
#define WESTERNZOBRIST_H

#include "zobrist.h"
#include <QMutex>

namespace Chess {

/*! \brief Zobrist keys for Western chess variants */
class LIB_EXPORT WesternZobrist : public Zobrist
{
	public:
		/*!
		 * Creates a new uninitialized WesternZobrist object.
		 *
		 * \param keys An array of zobrist keys that can be used
		 * instead of the random numbers generated by the Zobrist
		 * class.
		 */
		WesternZobrist(const quint64* keys = nullptr);

		// Inherited from Zobrist
		virtual void initialize(int squareCount,
					int pieceTypeCount);
		virtual quint64 side() const;
		virtual quint64 piece(const Piece& piece, int square) const;

		/*!
		 * Returns the zobrist value for an en-passant target
		 * at \a square.
		 */
		//virtual quint64 enpassant(int square) const;
		/*!
		 * Returns the zobrist value for player \a side's
		 * castling rook at \a square.
		 */
		//virtual quint64 castling(int side, int square) const;

	private:
		//int m_castlingIndex;
		int m_pieceIndex;
		QMutex m_mutex;
};

} //namespace Chess
#endif // WESTERNZOBRIST_H
