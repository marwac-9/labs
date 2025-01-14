//
// Created by marwac-9 on 9/16/15.
//
#include "app.h"
#include "gl_window.h"
#include "MyMathLib.h"

#include <vector>
class BoundingBox;
class Object;
class OBJ;
class Vao;
class HalfEdgeMesh;
class Camera;

namespace Subdivision
{
	class SubdivisionApp : public Core::App
	{
	public:
		/// constructor
		SubdivisionApp();
		/// destructor
		~SubdivisionApp();

		/// open app
		bool Open();
		/// run app
		void Run();
	private:
		void Clear();
		void ClearSubdivisionData();

		void Draw();
		void InitGL();
		void KeyCallback(int key, int scancode, int action, int mods);
		void MouseCallback(double mouseX, double mouseY);
		void Monitor(Display::Window* window);
		void SetUpCamera();
		void LoadScene1();
		void LoadScene2();
		void LoadScene3();
		void LoadScene4();
		void LoadScene5();
		void LoadScene6();
		void LoadScene7();
		void LoadScene8();
		void LoadScene9();
		void Subdivide(OBJ* objToSubdivide);
		bool altButtonToggle = true;
		bool minimized = false;
		bool isLeftMouseButtonPressed = false;
		bool running = false;
		bool wireframe = false;
		Display::Window* window;
		double leftMouseX;
		double leftMouseY;
		int windowWidth;
		int windowHeight;
		float windowMidX;
		float windowMidY;
		float near = 0.1f;
		float far = 2000.f;
		float fov = 45.0f;
		Camera* currentCamera;

		int objectsRendered = 0;

		std::vector<OBJ*> dynamicOBJs;
		std::vector<Vao*> dynamicVaos;
		std::vector<HalfEdgeMesh*> dynamicHEMeshes;
	};
} // namespace 
