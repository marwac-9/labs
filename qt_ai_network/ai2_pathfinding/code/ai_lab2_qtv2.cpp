#include "ai_lab2_qtv2.h"
#include "HalfEdgeMesh.h"
#include "Face.h"
#include "Edge.h"
#include "Vertex.h"
#include "AStar.h"
#include "Breadth_First.h"
#include "Depth_first.h"
#include "Dijkstra.h"
#include "TurnLeft.h"
#include "Random.h"
#include "dirent.h"
#include <algorithm> 
#include <QElapsedTimer>
#include <time.h>
#include <set>

using namespace mwm;

Ai_Lab2_qtv2::Ai_Lab2_qtv2(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
	QObject::connect(ui.listWidget, SIGNAL(itemSelectionChanged()), this, SLOT(loadSelectedMap()));
	QObject::connect(ui.pushButtonOptimize, SIGNAL(clicked()), this, SLOT(optimize()));
	QObject::connect(ui.checkBoxQuad, SIGNAL(stateChanged(int)), this, SLOT(quad(int)));
	QObject::connect(ui.pushButtonPath, SIGNAL(clicked()), this, SLOT(calculateAndDrawPath()));
	scene = new QGraphicsScene(this);
	ui.graphicsView->setScene(scene);
	loadMapsPaths("Maps");
	if (mapPaths.size() > 0)
	{
		ui.listWidget->setCurrentRow(0);
	}
	srand(time(NULL));
	
	
	//scene->addItem(sim);
	//mesh = new HalfEdgeMesh();
	//mesh->Construct("Maps\\Map1.txt");
	
	//mesh->quadrangulate();
	//mesh->optimizeMesh();

	//addAllTriangles();
	/*Face* foundFace = mesh->findNode(mesh->endFace->getMidPointMiniMaxi());
	if (foundFace != NULL)
	{
		drawface(foundFace, QColor(0,255,0,100));
	}*/
	//drawfaces(mesh->faces, QColor(255,0,0,100));
	//std::vector<Face*> path = BreadthFirst().BFS(mesh->startFace, mesh->endFace);
	//std::vector<Face*> path = DepthFirst().DFS(mesh->startFace, mesh->endFace);
	//std::vector<Face*> path = Dijkstra().DijSearch(mesh->faces, mesh->startFace, mesh->endFace);
	//std::vector<Face*> path = AStar().AStarSearch(mesh->faces, mesh->findNode(mesh->startFacePos), mesh->findNode(mesh->endFacePos));
	//std::vector<Face*> path = TurnLeft().TurnLeftSearch(mesh->faces, mesh->findNode(mesh->startFacePos), mesh->findNode(mesh->endFacePos));
	//drawfaces(path, QColor(255,255,0,200));
	//drawpath(path, mesh->startFacePos, mesh->endFacePos);
	
}

Ai_Lab2_qtv2::~Ai_Lab2_qtv2()
{

}

void Ai_Lab2_qtv2::addAllTriangles()
{
	for (size_t i = 0; i < mesh->faces.size(); i++)
	{
		Face* curFace = mesh->faces.at(i);
		QPolygonF polygon = buildPolygon(curFace);

		// Add the polygon to the scene
		QGraphicsPolygonItem* PolygonItem = scene->addPolygon(polygon);
		polygons.push_back(PolygonItem);
	}
}

void Ai_Lab2_qtv2::drawfaces(const std::vector<Face*> &path, QColor& color)
{
	for (size_t i = 0; i < path.size(); i++)
	{
		QGraphicsPolygonItem* PolygonItem = drawface(path.at(i), color);
		polygons.push_back(PolygonItem);
	}
}

void Ai_Lab2_qtv2::drawgoals(const std::vector<Face*> &path, QColor& color)
{
	for (size_t i = 0; i < path.size(); i++)
	{
		QGraphicsPolygonItem* PolygonItem = drawface(path.at(i), color);
		goals.push_back(PolygonItem);
	}
}

void Ai_Lab2_qtv2::clearMap()
{
	for (size_t i = 0; i < polygons.size(); i++)
	{
		scene->removeItem(polygons.at(i));
	}
	polygons.clear();
	for (size_t i = 0; i < linepath.size(); i++)
	{
		scene->removeItem(linepath.at(i));
	}
	linepath.clear();
	for (size_t i = 0; i < facepath.size(); i++)
	{
		scene->removeItem(facepath.at(i));
	}
	facepath.clear();
}


void Ai_Lab2_qtv2::drawfaces(const std::list<Face*>& faces, QColor& color)
{
	for (std::list<Face*>::const_iterator iterator = faces.begin(), end = faces.end(); iterator != end; ++iterator) {
		QGraphicsPolygonItem* PolygonItem = drawface(*iterator, color);
		polygons.push_back(PolygonItem);
	}
}

void Ai_Lab2_qtv2::drawPathFaces(const std::list<Face*>& faces, QColor& color)
{
	for (std::list<Face*>::const_iterator iterator = faces.begin(), end = faces.end(); iterator != end; ++iterator) {
		QGraphicsPolygonItem* PolygonItem = drawface(*iterator, color);
		facepath.push_back(PolygonItem);
	}
}

void Ai_Lab2_qtv2::drawPathFaces(const std::vector<Face*>& path, QColor& color)
{
	for (size_t i = 0; i < path.size(); i++)
	{
		QGraphicsPolygonItem* PolygonItem = drawface(path.at(i), color);
		facepath.push_back(PolygonItem);
	}
}

QGraphicsPolygonItem* Ai_Lab2_qtv2::drawface(Face* face, QColor& color)
{
	QPolygonF polygon = buildPolygon(face);

	QBrush brush(color);
	QPen pen(Qt::black);
	// Add the polygon to the scene
	QGraphicsPolygonItem* PolygonItem = scene->addPolygon(polygon, pen, brush);
	return PolygonItem;
}

void Ai_Lab2_qtv2::drawpath(const std::vector<Face*> &path, const Vector2 &start, const Vector2 &end)
{

	QPen pen(Qt::yellow);
	QLine line;
	line.setLine(start.vect[0], start.vect[1], path.back()->getMidPointMiniMaxi().vect[0], path.back()->getMidPointMiniMaxi().vect[1]);
	QGraphicsLineItem* lineitem = scene->addLine(line, pen);
	linepath.push_back(lineitem);
	line.setLine(end.vect[0], end.vect[1], path.front()->getMidPointMiniMaxi().vect[0], path.front()->getMidPointMiniMaxi().vect[1]);
	lineitem = scene->addLine(line, pen);
	linepath.push_back(lineitem);
	for (size_t i = 0; i < path.size() - 1; i++)
	{
		QPen pen(Qt::green);
		QLine line;
		Vector2 node1 = path.at(i)->getMidPointMiniMaxi();
		Vector2 node2 = path.at(i + 1)->getMidPointMiniMaxi();
		line.setLine(node1.vect[0], node1.vect[1], node2.vect[0], node2.vect[1]);
		QGraphicsLineItem* lineitem = scene->addLine(line, pen);
		linepath.push_back(lineitem);
	}
}

QPolygonF Ai_Lab2_qtv2::buildPolygon(Face* face)
{
	QPolygonF polygon;
	Edge* currentEdge = face->edge;
	//Looping through every edge in the current node
	do
	{
		Vertex* vert1 = currentEdge->vertex;
		polygon.append(QPointF(vert1->pos.vect[0], vert1->pos.vect[1]));
		currentEdge = currentEdge->next;

	} while (currentEdge != face->edge);
	return polygon;
}
void Ai_Lab2_qtv2::loadSelectedMap()
{
	if (mesh != NULL)
	{
		delete mesh;
	}
	int current = ui.listWidget->currentRow();
	mesh = new HalfEdgeMesh();
	QString mapPath = mapPaths.at(current);
	std::string mapstd = mapPath.toStdString();
	mesh->Construct(mapstd.c_str());
	if (ui.checkBoxQuad->isChecked())
	{
		mesh->quadrangulate();
		drawmap();
	}
	else
	{
		drawmap();
	}
}

void Ai_Lab2_qtv2::drawmap()
{
	clearScene();
	drawfaces(mesh->faces, QColor(255, 0, 0, 100));
	start = drawface(mesh->startFace, QColor(0, 255, 0, 200));
	
	drawgoals(mesh->goals, QColor(255, 255, 0, 200));
}
void Ai_Lab2_qtv2::loadMapsPaths(const char * path) {

	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir(path)) != NULL) {
		/* print all the files and directories within directory */
		while ((ent = readdir(dir)) != NULL) {
			if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0)
			{
				QString slash = "/";
				QString fileWithPath = path + slash + QString(ent->d_name);
				mapPaths.push_back(fileWithPath);
			}
			//printf("%s\n", ent->d_name);
		}
		closedir(dir);
	}
	else {
		/* could not open directory */
		perror("could not open directory");
	}

	for (size_t i = 0; i < mapPaths.size(); i++)
	{
		ui.listWidget->addItem(mapPaths.at(i));
	}
}

void Ai_Lab2_qtv2::clearScene()
{
	scene->clear();
	polygons.clear();
	linepath.clear();
	facepath.clear();
	goals.clear();
	start = NULL;
}

void Ai_Lab2_qtv2::optimize()
{
	if (mesh != NULL)
	{
		if (ui.checkBoxQuad->isChecked())
		{
			mesh->optimizeMesh();
		}
		else
		{
			mesh->optimizeMesh();
			mesh->optimizeMesh();
		}
		clearMap();
		drawfaces(mesh->faces, QColor(255, 0, 0, 100));
	}
}

void Ai_Lab2_qtv2::quad(int state)
{
	if (state == 2)
	{
		loadSelectedMap();
		mesh->quadrangulate();
		clearMap();
		drawfaces(mesh->faces, QColor(255, 0, 0, 100));
	}
	else
	{
		loadSelectedMap();
		clearMap();
		drawfaces(mesh->faces, QColor(255, 0, 0, 100));
	}
}

float Ai_Lab2_qtv2::calculatePathLength(const std::vector<Face*>& path)
{

	float length = 0;
	for (size_t i = 0; i < path.size() - 1; i++)
	{
		length += (path.at(i)->getMidPointMiniMaxi() - path.at(i + 1)->getMidPointMiniMaxi()).vectLengt();
	}
	return length;
}


int Ai_Lab2_qtv2::indexOfShortestGoal(const std::vector<float>& lengths, float mini)
{
	for (size_t i = 0; i < lengths.size(); i++)
	{
		if (lengths.at(i) == mini)
		{
			return i;
		}
	}
	return -1;
}

void Ai_Lab2_qtv2::calculateAndDrawPath()
{
	QElapsedTimer timer;
	std::vector<Face*> finalpath;
	Vector2 goal;
	std::vector<std::vector<Face*>> paths;
	
	if (ui.radioAStar->isChecked())
	{
		timer.start();
		for (size_t i = 0; i < mesh->goals.size(); i++)
		{
			Face* startFace = mesh->findNode(mesh->startFacePos);
			Face* endFace = mesh->findNode(mesh->goalsPos.at(i));
			std::vector<Face*> path = AStar().AStarSearch(startFace, endFace);
			paths.push_back(path);
		}			
	}
	else if (ui.radioDijkstra->isChecked())
	{
		
		std::set<Face*> goalsSet;
		Face* startFace = mesh->findNode(mesh->startFacePos);
		for (size_t i = 0; i < mesh->goals.size(); i++)
		{
			Face* endFace = mesh->findNode(mesh->goalsPos.at(i));
			goalsSet.insert(endFace);
		}
		timer.start();
		std::vector<Face*> path = Dijkstra().DijSearch(startFace, goalsSet);
		paths.push_back(path);
	}
	else if (ui.radioBreadth->isChecked())
	{
		std::set<Face*> goalsSet;
		Face* startFace = mesh->findNode(mesh->startFacePos);
		for (size_t i = 0; i < mesh->goals.size(); i++)
		{
			Face* endFace = mesh->findNode(mesh->goalsPos.at(i));
			goalsSet.insert(endFace);
		}
		timer.start();
		std::vector<Face*> path = BreadthFirst().BFS(startFace, goalsSet);
		paths.push_back(path);
	}
	else if (ui.radioDepth->isChecked())
	{
		timer.start();
		for (size_t i = 0; i < mesh->goals.size(); i++)
		{
			Face* startFace = mesh->findNode(mesh->startFacePos);
			Face* endFace = mesh->findNode(mesh->goalsPos.at(i));
			std::vector<Face*> path = DepthFirst().DFS(startFace, endFace);
			paths.push_back(path);
		}
	}
	else if (ui.radioTurnLeft->isChecked())
	{
		timer.start();
		for (size_t i = 0; i < mesh->goals.size(); i++)
		{
			Face* startFace = mesh->findNode(mesh->startFacePos);
			Face* endFace = mesh->findNode(mesh->goalsPos.at(i));
			std::vector<Face*> path = TurnLeft().TurnLeftSearch(mesh->faces, startFace, endFace);
			paths.push_back(path);
		}
	}
	else if (ui.radioRandom->isChecked())
	{
		timer.start();
		for (size_t i = 0; i < mesh->goals.size(); i++)
		{
			Face* startFace = mesh->findNode(mesh->startFacePos);
			Face* endFace = mesh->findNode(mesh->goalsPos.at(i));
			std::vector<Face*> path = Random().RandomSearch(mesh->faces, startFace, endFace);
			paths.push_back(path);
		}
	}
	if (paths.size() > 0)
	{
		float mini = 999999999.0f;
		std::vector<float> lengths;
		for (size_t i = 0; i < paths.size(); i++)
		{
			float length = calculatePathLength(paths.at(i));
			lengths.push_back(length);
			mini = min(length, mini);
		}
		int index = indexOfShortestGoal(lengths, mini);
		ui.doublePathLength->setValue(mini);
		ui.doubleTimeElapsed->setValue(timer.elapsed());
		if (index != -1)
		{
			finalpath = paths.at(index);
			//we make sure the node at the end of path(which is in the front of the container) is one of the goals and then we assign goal to that node position
			//needed for dijkstra as it only does one search while others do number of goals searches and return more paths 
			for (size_t z = 0; z < mesh->goals.size(); z++)
			{
				Face* goalTest = mesh->findNode(mesh->goalsPos.at(z));
				if (goalTest == finalpath.front())
				{
					goal = mesh->goalsPos.at(z);
					break;
				}
			}
		}
		if (finalpath.size() > 0)
		{
			clearMap();
			drawfaces(mesh->faces, QColor(255, 0, 0, 100));
			drawPathFaces(finalpath, QColor(0, 255, 255, 50));
			drawpath(finalpath, mesh->startFacePos, goal);
		}
	}
	
}