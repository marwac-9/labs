#pragma once
struct Telegram;

template<class entity_type>
class State
{
public:
	virtual ~State() {};
	//this will execute when the state is entered
	virtual void Enter(entity_type* entity) = 0;

	//this is the states normal update function
	virtual void Execute(entity_type* entity) = 0;

	//this will execute when the state is exited. 
	virtual void Exit(entity_type* entity) = 0;

	//this executes if the agent receives a message from the 
	//message dispatcher
	virtual bool OnMessage(entity_type* entity, const Telegram& msg) = 0;

private:

};

