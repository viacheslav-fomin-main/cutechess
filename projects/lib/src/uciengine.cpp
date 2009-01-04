/*
    This file is part of Cute Chess.

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

#include <QString>
#include <QStringList>
#include <QDebug>

#include "uciengine.h"
#include "chessboard/chessboard.h"
#include "chessboard/chessmove.h"
#include "timecontrol.h"


UciEngine::UciEngine(QIODevice* ioDevice,
                     Chess::Board* chessboard,
                     const TimeControl& timeControl,
                     QObject* parent)
	: ChessEngine(ioDevice, chessboard, timeControl, parent)
{
	setName("UciEngine");
	
	// Tell the engine to turn on Uci mode
	write("uci");

	// Don't send any commands to the engine until it's initialized.
	m_isReady = false;
}

UciEngine::~UciEngine()
{
	//write("quit");
}

void UciEngine::newGame(Chess::Side side, ChessPlayer* opponent)
{
	ChessPlayer::newGame(side, opponent);
	m_moves.clear();
	m_startFen = m_chessboard->fenString();
	
	write("ucinewgame");
	write(QString("position fen ") + m_startFen);
}

void UciEngine::makeMove(const Chess::Move& move)
{
	QString moveString = m_chessboard->moveString(move, m_notation);
	
	m_moves.append(moveString);
	write(QString("position fen ") + m_startFen +
		QString(" moves ") + m_moves.join(" "));
}

void UciEngine::go()
{
	const ChessPlayer* whitePlayer;
	const ChessPlayer* blackPlayer;
	if (side() == Chess::White)
	{
		whitePlayer = this;
		blackPlayer = m_opponent;
	}
	else if (side() == Chess::Black)
	{
		whitePlayer = m_opponent;
		blackPlayer = this;
	}
	else
		qFatal("Player %s doesn't have a side", qPrintable(m_name));
	
	const TimeControl& whiteTimeControl = whitePlayer->timeControl();
	const TimeControl& blackTimeControl = blackPlayer->timeControl();
	
	QString command = "go";
	if (m_timeControl.timePerMove() > 0)
		command += QString(" movetime ") + QString::number(m_timeControl.timePerMove());
	else
	{
		command += QString(" wtime ") + QString::number(whiteTimeControl.timeLeft());
		command += QString(" btime ") + QString::number(blackTimeControl.timeLeft());
		if (whiteTimeControl.timeIncrement() > 0)
			command += QString(" winc ") + QString::number(whiteTimeControl.timeIncrement());
		if (blackTimeControl.timeIncrement() > 0)
			command += QString(" binc ") + QString::number(blackTimeControl.timeIncrement());
		if (m_timeControl.movesLeft() > 0)
			command += QString(" movestogo ") + QString::number(m_timeControl.movesLeft());
	}
	write(command);
	
	ChessPlayer::go();
}

ChessEngine::ChessProtocol UciEngine::protocol() const
{
	return ChessEngine::Uci;
}

void UciEngine::ping()
{
	if (m_isReady)
	{
		write("isready");
		m_isReady = false;
	}
}

void UciEngine::parseLine(const QString& line)
{
	QString command = line.section(' ', 0, 0);
	QString args = line.right(line.length() - command.length() - 1);

	if (command == "bestmove")
	{
		QString moveString = args.section(' ', 0, 0);
		if (moveString.isEmpty())
			moveString = args;

		m_moves.append(moveString);
		Chess::Move move = m_chessboard->moveFromString(moveString);
		emit moveMade(move);
	}
	else if (command == "uciok")
	{
		if (!m_initialized)
		{
			Q_ASSERT(!m_isReady);
			m_initialized = true;
			m_isReady = true;

			// TODO: Send the engine the "setoption" commands

			ping();
		}
	}
	else if (command == "readyok")
	{
		if (!m_isReady)
		{
			m_isReady = true;
			flushWriteBuffer();
		}
	}
	else if (command == "id")
	{
		QString tag = args.section(' ', 0, 0);
		QString tagVal = args.section(' ', 1);
		
		if (tag == "name")
			m_name = tagVal;
	}
}
