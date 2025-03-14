#pragma once 
#include <map>
class BaseGameEntity;

//provide easy access
//#define EntityMgr EntityManager::Instance()



class EntityManager
{

private:
	typedef std::map<int, BaseGameEntity*> EntityMap;
	//to facilitate quick lookup the entities are stored in a std::map, in which
	//pointers to entities are cross referenced by their identifying number
	

	EntityManager();
	~EntityManager();

	//copy ctor and assignment should be private
	EntityManager(const EntityManager&);
	EntityManager& operator=(const EntityManager&);

public:
	static EntityManager* Instance();
	EntityMap entityMap;
	//this method stores a pointer to the entity in the std::vector
	//m_Entities at the index position indicated by the entity's ID
	//(makes for faster access)
	void RegisterEntity(BaseGameEntity* newEntity);

	//returns a pointer to the entity with the ID given as a parameter
	BaseGameEntity* GetEntityFromID(int id) const;
	BaseGameEntity* GetEntityFromName(std::string name) const;
	std::string GetEntityName(int id) const;

	//this method removes the entity from the list
	void RemoveEntity(BaseGameEntity* entity);

	void Update(BaseGameEntity* entity);

	void UpdateAll();

	int EntityCount();
};