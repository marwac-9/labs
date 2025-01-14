//
// Created by marwac-9 on 9/16/15.
//
#include "PickingApp.h"
#include "GraphicsManager.h"
#include "GraphicsStorage.h"
#include "Node.h"
#include "Material.h"
#include "OBJ.h"
#include "SceneGraph.h"
#include "ShaderManager.h"
#include "DebugDraw.h"
#include "PhysicsManager.h"
#include "FBOManager.h"
#include "Camera.h"
#include "Frustum.h"
#include "Render.h"
#include "ParticleSystem.h"
#include "PointSystem.h"
#include "BoundingBoxSystem.h"
#include "LineSystem.h"
#include "RigidBody.h"
#include "CameraManager.h"
#include <string>
#include <algorithm>
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <fstream>
#include "FrameBuffer.h"
#include <chrono>
#include "DirectionalLight.h"
#include "SpotLight.h"
#include "PointLight.h"
#include "Texture.h"
#include "Times.h"
#include "InstanceSystem.h"
#include <ctime> 
#include <sstream>
#include "stb_image_write.h"
#include "FastInstanceSystem.h"
#include "ImGuiWrapper.h"
#include <imgui.h>
#include "RenderBuffer.h"


using namespace Display;
namespace Picking
{

	//------------------------------------------------------------------------------
	/**
	*/
	PickingApp::PickingApp()
	{
		// empty
	}

	//------------------------------------------------------------------------------
	/**
	*/
	PickingApp::~PickingApp()
	{
		// empty
	}

	//------------------------------------------------------------------------------
	/**
	*/
	bool
		PickingApp::Open()
	{
		App::Open();
		this->window = new Display::Window;

		window->SetKeyPressFunction([this](int32 key, int32 scancode, int32 action, int32 mode)
		{
			if (action == GLFW_PRESS)
			{
				if (key == GLFW_KEY_ESCAPE) {
					running = false;
				}
				else if (key == GLFW_KEY_LEFT_ALT) {
					if (applicationInputEnabled) {
						applicationInputEnabled = false;
						window->SetCursorMode(GLFW_CURSOR_NORMAL);
						currentCamera->holdingForward = false;
						currentCamera->holdingBackward = false;
						currentCamera->holdingRight = false;
						currentCamera->holdingLeft = false;
						currentCamera->holdingUp = false;
						currentCamera->holdingDown = false;
					}
					else {
						applicationInputEnabled = true;
						window->SetCursorMode(GLFW_CURSOR_DISABLED);
						window->SetCursorPos(windowWidth / 2.f, windowHeight / 2.f);
					}
				}
			}
			if (applicationInputEnabled) KeyCallback(key, scancode, action, mode);
			
		});

		window->SetMouseMoveFunction([this](double mouseX, double mouseY)
		{
			if (applicationInputEnabled) MouseCallback(mouseX, mouseY);
		});

		window->SetWindowSizeFunction([this](int width, int height)
		{
			if (!minimized)
			{
				this->windowWidth = width;
				this->windowHeight = height;
				this->windowMidX = windowWidth / 2.0f;
				this->windowMidY = windowHeight / 2.0f;
				this->window->SetSize(this->windowWidth, this->windowHeight);
				FBOManager::Instance()->UpdateTextureBuffers(this->windowWidth, this->windowHeight);
				currentCamera->UpdateSize(width, height);
			}
		});

		window->SetMousePressFunction([this](int button, int action, int mods)
		{
			if (applicationInputEnabled)
			{
				if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
				{
					isLeftMouseButtonPressed = true;
					if (currentScene == scene3Loaded) FireLightProjectile();
				}
				else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
				{
					isLeftMouseButtonPressed = false;
				}
				else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
				{

				}
				else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
				{
				}
				else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS)
				{
					SceneGraph::Instance()->InitializeSceneTree();
					//GraphicsManager::ReloadShader("directionalLightPBR");
					GraphicsManager::ReloadShaders();
				}
				else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
				{
				}
			}
		});

		window->SetWindowIconifyFunction([this](int iconified) {
			if (iconified) minimized = true;
			else minimized = false;
		});

		window->SetDragAndDropFunction([this](int count, const char** paths) {
			printf("nr of files: %d\n", count);
			for (int i = 0; i < count; i++)
			{
				printf("file nr: %d, path: %s\n", i, paths[i]);
			}
		});

		if (this->window->Open())
		{
			running = true;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------
	/**
	*/
	void
		PickingApp::Run()
	{
		window->SetTitle("Main Engine Playground");
		stbi_flip_vertically_on_write(true);
		InitGL();

		SetUpBuffers(this->windowWidth, this->windowHeight);

		ImGuiWrapper ImGuiWrapper(window);

		GraphicsManager::LoadAllAssets();
		//DebugDraw::Instance()->LoadPrimitives();
		Render::Instance()->InitializeShderBlockDatas();
		//camera rotates based on mouse movement, setting initial mouse pos will always focus camera at the beginning in specific position
		window->SetCursorPos(windowMidX, windowMidY + 100);
		window->SetCursorMode(GLFW_CURSOR_DISABLED);

		SetUpCamera();

		LoadScene3();

		double customIntervalTime = 0;
		SceneGraph::Instance()->Update();

		//glfwSwapInterval(0); //unlock fps

		std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
		std::chrono::duration<double> elapsed_seconds;

		GLuint radianceShader = GraphicsStorage::shaderIDs["radianceHDRSkyboxUnwrap"];
		GLuint irradianceShader = GraphicsStorage::shaderIDs["irradianceFromEnvironmentMap"];
		GLuint prefilterShader = GraphicsStorage::shaderIDs["prefilterHDRMap"];
		GLuint brdfShader = GraphicsStorage::shaderIDs["brdf"];
		GLuint geometryShader = GraphicsStorage::shaderIDs["geometry"];
		GLuint geometryInstancedShader = GraphicsStorage::shaderIDs["geometryInstanced"];
		GLuint pointLightShader = GraphicsStorage::shaderIDs["pointLightPBR"];
		GLuint pointLightShadowShader = GraphicsStorage::shaderIDs["pointLightShadowPBR"];
		GLuint spotLightShader = GraphicsStorage::shaderIDs["spotLightPBR"];
		GLuint spotLightShadowShader = GraphicsStorage::shaderIDs["spotLightShadowPBR"];
		GLuint directionalLightShader = GraphicsStorage::shaderIDs["directionalLightPBR"];
		GLuint directionalLightShadowShader = GraphicsStorage::shaderIDs["directionalLightShadowPBR"];
		GLuint ambientLightShader = GraphicsStorage::shaderIDs["ambientLightPBR"];
		GLuint skyboxShader = GraphicsStorage::shaderIDs["skybox"];
		GLuint fastBlurShader = GraphicsStorage::shaderIDs["fastBlur"];
		GLuint hdrBloomShader = GraphicsStorage::shaderIDs["hdrBloom"];

		Render::Instance()->captureTextureToCubeMap(radianceShader, captureFBO, GraphicsStorage::textures["newport_loft"], envCubeMap);
		envCubeMap->ActivateAndBind(0);
		envCubeMap->GenerateMipMaps();
		envCubeMap->SetDefaultParameters();

		Render::Instance()->captureTextureToCubeMap(irradianceShader, captureFBO, envCubeMap, irradianceCubeMap);
		Render::Instance()->captureTextureToCubeMapWithMips(prefilterShader, captureFBO, envCubeMap, prefilteredHDRMap);
		Render::Instance()->captureToTexture2D(brdfShader, captureFBO, brdfTexture);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		while (running)
		{
			FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, 0);
			glDepthMask(GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);

			glDisable(GL_BLEND);
			this->window->Update();
			if (minimized) continue;

			ImGuiWrapper.NewFrame();

			Times::Instance()->Update(glfwGetTime());

			Monitor(this->window);

			start = std::chrono::high_resolution_clock::now();

			CameraManager::Instance()->Update(Times::Instance()->deltaTime);
			FrustumManager::Instance()->ExtractPlanes(CameraManager::Instance()->ViewProjection);

			if (currentScene == scene4Loaded) LoadScene4();
			if (customIntervalTime >= 0.5) {
				customIntervalTime = 0.0;
				if (currentScene == scene2Loaded)
				{
					/*
					int pI = rand() % SceneGraph::Instance()->pickingList.size();
					std::map<unsigned int, Object*>::iterator it = SceneGraph::Instance()->pickingList.begin();
					std::advance(it, pI);

					Object* parent = it->second;
					SceneGraph::Instance()->addObjectTo(parent, "icosphere", SceneGraph::Instance()->generateRandomIntervallVectorCubic(-3, 3));
					*/
				}
				if (currentScene == scene9Loaded)
				{
					//SpawnSomeLights();
				}
			}

			if (!Times::Instance()->paused)
			{
				SceneGraph::Instance()->Update();

				if (!pausedPhysics)
				{
					if (currentScene == scene7Loaded) Vortex();
					//if (currentScene == scene3Loaded) Vortex();

					PhysicsManager::Instance()->Update(Times::Instance()->dtInv);
				}
			}

			SceneGraph::Instance()->FrustumCulling();

			end = std::chrono::high_resolution_clock::now();
			elapsed_seconds = end - start;
			updateTime = elapsed_seconds.count();

			Render::Instance()->UpdateShaderBlockDatas();
			ImGui::ShowDemoWindow();
			GenerateGUI();
			objectsRendered = Render::Instance()->drawGeometry(geometryShader, SceneGraph::Instance()->renderList, geometryBuffer);
			instancedGeometryDrawn = Render::Instance()->drawInstancedGeometry(geometryInstancedShader, SceneGraph::Instance()->instanceSystemComponents, geometryBuffer);
			instancedGeometryDrawn += Render::Instance()->drawFastInstancedGeometry(geometryInstancedShader, SceneGraph::Instance()->fastInstanceSystemComponents, geometryBuffer);
			//Render::Instance()->drawGSkybox(geometryBuffer, captureFBO->textures[0]);

			if (applicationInputEnabled) PickingTest();
			//Render::Instance()->captureTextureToCubeMap(shader, captureFBO, captureRBO, captureFBO->textures[0], irradianceCubeMap, captureVPs);
			
			lightsRendered = Render::Instance()->drawLight(pointLightShader, pointLightShadowShader, spotLightShader, spotLightShadowShader, directionalLightShader, directionalLightShadowShader, lightAndPostBuffer, geometryBuffer);
			
			lightsRendered += Render::Instance()->drawAmbientLight(ambientLightShader, lightAndPostBuffer, geometryBuffer->textures, pbrEnvTextures);

			//if (skybox != nullptr) Render::Instance()->drawSkybox(lightAndPostBuffer, GraphicsStorage::cubemaps[0]);
			Render::Instance()->drawSkybox(skyboxShader, lightAndPostBuffer, envCubeMap);
			//Render::Instance()->drawSkybox(skyboxShader, lightAndPostBuffer, GraphicsStorage::cubemaps["c2"]);

			if (drawLines) DebugDraw::Instance()->DrawFastLineSystems(lightAndPostBuffer->handle);  //<--- to light pass
			if (drawPoints) DebugDraw::Instance()->DrawFastPointSystems(lightAndPostBuffer->handle);  //<--- to light pass
			if (drawParticles) DrawParticles();  //<--- to light pass

			if (post)
			{
				blurredBrightTexture = Render::Instance()->MultiBlur(brightLightTexture, bloomLevel, blurBloomSize, fastBlurShader);
				Render::Instance()->drawHDR(hdrBloomShader, finalColorTexture, blurredBrightTexture); //we draw color to screen here
				BlitDepthToScreenPass(); //we blit depth so we can use it in forward rendering on top of deffered
			}
			else
			{
				BlitToScreenPass(); //final color and depth
			}

			if (drawBB) DebugDraw::Instance()->DrawBoundingBoxes();

			//Render::Instance()->drawHDRequirectangular(GraphicsStorage::textures[23]);

			DebugDraw::Instance()->DrawCrossHair();

			if (drawMaps) DrawGeometryMaps(windowWidth, windowHeight);

			ImGuiWrapper.Render();
			customIntervalTime += Times::Instance()->deltaTime;

			this->window->SwapBuffers();

		}
		GraphicsStorage::Clear();
		this->window->Close();
	}

	void
	PickingApp::Clear()
	{
		particleSystems.clear();
		SceneGraph::Instance()->Clear();
		PhysicsManager::Instance()->Clear();
		GraphicsStorage::ClearMaterials();
		DebugDraw::Instance()->Clear();
		lastPickedObject = nullptr;
		firstObject = nullptr;
		secondObject = nullptr;
		spotLight1 = nullptr;
		spotLightComp = nullptr;
		directionalLightComp = nullptr;
		directionalLightComp2 = nullptr;
		directionalLightComp3 = nullptr;
		directionalLightComp4 = nullptr;
		pointLightTest = nullptr;
	}

	void
	PickingApp::DrawParticles()
	{
		FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, lightAndPostBuffer->handle); //we bind the lightandposteffect buffer for drawing
		
		GLuint particleShader = GraphicsStorage::shaderIDs["softparticle"];
		//GLuint particleShader = GraphicsStorage::shaderIDs["particle"];
		ShaderManager::Instance()->SetCurrentShader(particleShader);

		depthTexture->ActivateAndBind(1);

		GLuint soft = glGetUniformLocation(particleShader, "softScale");
		glUniform1f(soft, softScale);

		GLuint contrast = glGetUniformLocation(particleShader, "contrastPower");
		glUniform1f(contrast, contrastPower);

		particlesRendered = 0;
		for (auto& pSystem : particleSystems) //particles not affected by light, rendered in forward rendering
		{
			if (pSystem->object->inFrustum) {
				particlesRendered += pSystem->Draw();
			}
		}
	}

	void
	PickingApp::KeyCallback(int key, int scancode, int action, int mods)
	{
		if (action == GLFW_PRESS)
		{
			if (key == GLFW_KEY_R) {
				//SceneGraph::Instance()->addRandomObject();
			}
			else if (key == GLFW_KEY_1) {
				LoadScene1();
			}
			else if (key == GLFW_KEY_2) {
				LoadScene2();
			}
			else if (key == GLFW_KEY_3) {
				LoadScene3();
			}
			else if (key == GLFW_KEY_4) {
				LoadScene4();
			}
			else if (key == GLFW_KEY_5) {
				LoadScene5();
			}
			else if (key == GLFW_KEY_6) {
				LoadScene6();
			}
			else if (key == GLFW_KEY_7) {
				LoadScene7();
			}
			else if (key == GLFW_KEY_8) {
				LoadScene8();
			}
			else if (key == GLFW_KEY_9) {
				LoadScene9();
			}
			else if (key == GLFW_KEY_0) {
				LoadScene0();
			}
			else if (key == GLFW_KEY_BACKSPACE)
			{
				if (lightsPhysics) lightsPhysics = false;
				else lightsPhysics = true;
			}
			else if (key == GLFW_KEY_TAB) {
				if (wireframe)
				{
					wireframe = false;
					printf("\nSHADED MODE\n");
					glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				}
				else
				{
					wireframe = true;
					printf("\nWIREFRAME MODE\n");
					glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				}

			}
			else if (key == GLFW_KEY_S && window->GetKey(GLFW_KEY_LEFT_CONTROL)) {
				auto it = --GraphicsStorage::objs.end();
				GraphicsManager::SaveToOBJ(it->second);
				std::cout << "Last Mesh Saved" << std::endl;
			}
			else if (key == GLFW_KEY_O) {
				if (drawBB) drawBB = false;
				else drawBB = true;

			}
			else if (key == GLFW_KEY_P)
			{
				if (Times::Instance()->paused) Times::Instance()->paused = false;
				else Times::Instance()->paused = true;
			}
			else if (key == GLFW_KEY_M)
			{
				if (drawMaps) drawMaps = false;
				else drawMaps = true;
			}
			else if (key == GLFW_KEY_K)
			{
				if (pausedPhysics) pausedPhysics = false;
				else pausedPhysics = true;
			}
			else if (key == GLFW_KEY_T)
			{
				Times::Instance()->timeModifier = 0.0;
			}
			else if (key == GLFW_KEY_F5)
			{
				if (DebugDraw::Instance()->debug) DebugDraw::Instance()->debug = false;
				else DebugDraw::Instance()->debug = true;
			}

			else if (key == GLFW_KEY_E)
			{
				Object* cube = SceneGraph::Instance()->addPhysicObject("cube", Vector3(0.0, 8.0, 0.0));
				cube->node->SetPosition(Vector3(0.0, (double)SceneGraph::Instance()->pickingList.size() * 2.0 - 10.0 + 0.001, 0.0));
				SceneGraph::Instance()->SwitchObjectMovableMode(cube, true);
			}
			else if (key == GLFW_KEY_L)
			{
				if (drawLines) drawLines = false;
				else drawLines = true;
			}
			else if (key == GLFW_KEY_N)
			{
				if (drawPoints) drawPoints = false;
				else drawPoints = true;
			}
			else if (key == GLFW_KEY_B)
			{
				if (drawParticles) drawParticles = false;
				else drawParticles = true;
			}
			else if (key == GLFW_KEY_PRINT_SCREEN)
			{
				GraphicsManager::DumpTextureSTB(lightAndPostBuffer, finalColorTexture);
			}
			else if (key == GLFW_KEY_KP_ADD) increment+=20;//Times::timeModifier += 0.0005;
			else if (key == GLFW_KEY_KP_SUBTRACT) increment-=20;//Times::timeModifier -= 0.0005;
		}
	}

	void
	PickingApp::Monitor(Display::Window* window)
	{
		//if (window->GetKey(GLFW_KEY_KP_ADD) == GLFW_PRESS) increment++;//Times::timeModifier += 0.0005;
		//if (window->GetKey(GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) increment--;//Times::timeModifier -= 0.0005;
		if (window->GetKey(GLFW_KEY_UP) == GLFW_PRESS) if (lastPickedObject) lastPickedObject->node->Translate(Vector3(0.f, 0.05f, 0.f));
		if (window->GetKey(GLFW_KEY_DOWN) == GLFW_PRESS) if (lastPickedObject) lastPickedObject->node->Translate(Vector3(0.f, -0.05f, 0.f));
		if (window->GetKey(GLFW_KEY_LEFT) == GLFW_PRESS) if (lastPickedObject) lastPickedObject->node->Translate(Vector3(0.05f, 0.f, 0.f));
		if (window->GetKey(GLFW_KEY_RIGHT) == GLFW_PRESS) if (lastPickedObject) lastPickedObject->node->Translate(Vector3(-0.05f, 0.f, 0.f));

		if (applicationInputEnabled)
		{
			currentCamera->holdingForward = (window->GetKey(GLFW_KEY_W) == GLFW_PRESS);
			currentCamera->holdingBackward = (window->GetKey(GLFW_KEY_S) == GLFW_PRESS);
			currentCamera->holdingRight = (window->GetKey(GLFW_KEY_D) == GLFW_PRESS);
			currentCamera->holdingLeft = (window->GetKey(GLFW_KEY_A) == GLFW_PRESS);
			currentCamera->holdingUp = (window->GetKey(GLFW_KEY_SPACE) == GLFW_PRESS);
			currentCamera->holdingDown = (window->GetKey(GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);
		}

		currentCamera->fov = fov;
		currentCamera->near = near;
		currentCamera->far = far;

		Quaternion qXangles = Quaternion(xAngles, Vector3(1.0, 0.0, 0.0));
		Quaternion qYangles = Quaternion(yAngles, Vector3(0.0, 1.0, 0.0));
		Quaternion spotTotalRotation = qYangles * qXangles;

		if (pointLightTest != nullptr)
		{
			pointLightTest->node->SetOrientation(spotTotalRotation);
			pointLightTest->node->SetPosition(Vector3(pposX, pposY, pposZ));
			pointLightTest->node->SetScale(Vector3(pointScale, pointScale, pointScale));
		}

		if (spotLight1 != nullptr)
		{
			spotLight1->node->SetOrientation(spotTotalRotation);
			spotLight1->node->SetPosition(Vector3(posX, posY, posZ));
			spotLightComp->SetCutOff(spotLightCutOff);
			spotLightComp->SetOuterCutOff(spotLightOuterCutOff);
			spotLightComp->SetRadius(spotSZ);
		}

		if (directionalLightComp != nullptr) 
		{
			Quaternion qXangled = Quaternion(xAngled, Vector3(1.0, 0.0, 0.0));
			Quaternion qYangled = Quaternion(yAngled, Vector3(0.0, 1.0, 0.0));
			Quaternion dirTotalRotation = qYangled * qXangled;
			directionalLightComp->object->node->SetOrientation(dirTotalRotation);
			directionalLightComp->object->node->SetPosition(currentCamera->GetPosition2());
		}

		if (directionalLightComp2 != nullptr)
		{
			Quaternion qXangled2 = Quaternion(xAngled2, Vector3(1.0, 0.0, 0.0));
			Quaternion qYangled2 = Quaternion(yAngled2, Vector3(0.0, 1.0, 0.0));
			Quaternion dirTotalRotation2 = qYangled2 * qXangled2;
			directionalLightComp2->object->node->SetOrientation(dirTotalRotation2);
			directionalLightComp2->object->node->SetPosition(currentCamera->GetPosition2());
		}

		if (directionalLightComp3 != nullptr)
		{
			Quaternion qXangled3 = Quaternion(xAngled3, Vector3(1.0, 0.0, 0.0));
			Quaternion qYangled3 = Quaternion(yAngled3, Vector3(0.0, 1.0, 0.0));
			Quaternion dirTotalRotation3 = qYangled3 * qXangled3;
			directionalLightComp3->object->node->SetOrientation(dirTotalRotation3);
			directionalLightComp3->object->node->SetPosition(currentCamera->GetPosition2());
		}

		if (directionalLightComp4 != nullptr)
		{
			Quaternion qXangled4 = Quaternion(xAngled4, Vector3(1.0, 0.0, 0.0));
			Quaternion qYangled4 = Quaternion(yAngled4, Vector3(0.0, 1.0, 0.0));
			Quaternion dirTotalRotation4 = qYangled4 * qXangled4;
			directionalLightComp4->object->node->SetOrientation(dirTotalRotation4);
			directionalLightComp4->object->node->SetPosition(currentCamera->GetPosition2());
		}
		if (currentScene == scene2Loaded)
		{
			Quaternion qX = Quaternion(xCubeAngle, Vector3(1, 0, 0));
			Quaternion qY = Quaternion(yCubeAngle, Vector3(0, 1, 0));
			Quaternion qZ = Quaternion(zCubeAngle, Vector3(0, 0, 1));

			Quaternion totalRot = qX * qY * qZ;
			
			if (lastPickedObject != nullptr)
			{
				lastPickedObject->node->SetOrientation(totalRot);
				lastPickedObject->materials[0]->SetShininess(cubeShininess);
			}
		}
		/*
		if (currentScene == scene0Loaded)
		{
			Quaternion qX = Quaternion(xCubeAngle, Vector3(1, 0, 0));
			Quaternion qY = Quaternion(yCubeAngle, Vector3(0, 1, 0));
			Quaternion qZ = Quaternion(zCubeAngle, Vector3(0, 0, 1));

			Quaternion totalRot = qX * qY * qZ;
			Matrix4 totalRotToMat = totalRot.ConvertToMatrix();

			Matrix4 xRot = Matrix4::rotateAngle(Vector3(1, 0, 0), xCubeAngle);
			Matrix4 yRot = Matrix4::rotateAngle(Vector3(0, 1, 0), yCubeAngle);
			Matrix4 zRot = Matrix4::rotateAngle(Vector3(0, 0, 1), zCubeAngle);
			Matrix4 totalMatRot = zRot * yRot * xRot;
			Quaternion totMatToQ = totalMatRot.toQuaternion();

			qXCube1->node->SetPosition(totalRot.getLeft()*10.0);
			qYCube1->node->SetPosition(totalRot.getUp()*10.0);
			qZCube1->node->SetPosition(totalRot.getForward()*10.0);

			qXCube1->node->SetOrientation(totalRot);
			qYCube1->node->SetOrientation(totalRot);
			qZCube1->node->SetOrientation(totalRot);

			qXCube2->node->SetPosition(totalMatRot.getLeft()*10.0);
			qYCube2->node->SetPosition(totalMatRot.getUp()*10.0);
			qZCube2->node->SetPosition(totalMatRot.getForward()*10.0);

			qXCube2->node->SetOrientation(totMatToQ);
			qYCube2->node->SetOrientation(totMatToQ);
			qZCube2->node->SetOrientation(totMatToQ);
		}
		*/
	}

	void
	PickingApp::PickingTest()
	{
		if (isLeftMouseButtonPressed)
		{
			window->GetCursorPos(&leftMouseX, &leftMouseY);

			//read pixel from picking texture
			unsigned int Pixel;
			//inverted y coordinate because glfw 0,0 starts at topleft while opengl texture 0,0 starts at bottomleft
			geometryBuffer->ReadPixelData((unsigned int)leftMouseX, this->windowHeight - (unsigned int)leftMouseY, 1, 1, GL_UNSIGNED_INT, &Pixel, pickingTexture);
			pickedID = Pixel;
			
			//std::cout << pickedID << std::endl;
			if (lastPickedObject != nullptr) //reset previously picked object color
			{
				lastPickedObject->materials[0]->color = Vector3F(0.f, 0.f, 0.f);
			}
			if (SceneGraph::Instance()->pickingList.find(pickedID) != SceneGraph::Instance()->pickingList.end())
			{
				lastPickedObject = SceneGraph::Instance()->pickingList[pickedID];
				lastPickedObject->materials[0]->color = Vector3F(0.0f, 0.5f, 0.5f);
				//lastPickedObject->materials[0]->SetDiffuseIntensity(80.f);
				Vector3F world_position;
				geometryBuffer->ReadPixelData((unsigned int)leftMouseX, this->windowHeight - (unsigned int)leftMouseY, 1, 1, GL_FLOAT, world_position.vect, worldPosTexture);
				Vector3 dWorldPos = Vector3(world_position.x, world_position.y, world_position.z);
				Vector3 impulse = (dWorldPos - currentCamera->GetPosition2()).vectNormalize();
				if (RigidBody* body = this->lastPickedObject->GetComponent<RigidBody>()) body->ApplyImpulse(impulse, 1.0, dWorldPos);
				if (!SceneGraph::Instance()->fastInstanceSystemComponents.empty())
				{
					//SceneGraph::Instance()->fastInstanceSystemComponents[0]->ReturnObject(lastPickedObject); //just a test
					//Object* obj = SceneGraph::Instance()->fastInstanceSystemComponents[0]->GetObject();
					for (size_t i = 0; i < 4; i++)
					{
						Object* obj = SceneGraph::Instance()->fastInstanceSystemComponents[0]->GetObject();
						if (i % 2 == 0)
						{
							SceneGraph::Instance()->fastInstanceSystemComponents[0]->ReturnObject(obj);
						}
					}
					
				}
			}
		}
	}

	void
	PickingApp::InitGL()
	{
		// grey background
		glClearColor(0.f, 0.f, 0.f, 1.0f);

		// Enable depth test
		glEnable(GL_DEPTH_TEST);
		// Accept fragment if it closer to the camera than the former one
		glDepthFunc(GL_LESS);

		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);

		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

		this->window->GetWindowSize(&this->windowWidth, &this->windowHeight);
		windowMidX = windowWidth / 2.0f;
		windowMidY = windowHeight / 2.0f;
	}

	void
	PickingApp::LoadScene1()
	{
		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		lightsPhysics = false;
		currentCamera->SetPosition(Vector3(0.f, 20.f, 60.f));
		//DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		Object* sphere = SceneGraph::Instance()->addPhysicObject("sphere", Vector3(0.f, 3.f, 0.f));//automatically registered for collision detection and response
		
		RigidBody* body = sphere->GetComponent<RigidBody>();
		body->SetIsKinematic(true);
		sphere->materials[0]->SetShininess(10.f);

		Object* tunnel = SceneGraph::Instance()->addObject("tunnel", Vector3(0.f, 0.f, 25.f));
		tunnel->materials[0]->AssignTexture(GraphicsStorage::textures["wood"]);
		//tunnel->materials[0]->SetSpecularIntensity(0.f);
		//tunnel->materials[0]->SetShininess(10.f);

		body = new RigidBody();
		tunnel->AddComponent(body);
		//tunnel->SetScale(Vector3(25.f, 2.f, 25.f));
		body->SetIsKinematic(true);
		PhysicsManager::Instance()->RegisterRigidBody(body);

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();
		directionalLightComp = directionalLight->GetComponent<DirectionalLight>();

		///when rendering lights only diffuse intensity and color is important as they are light power and light color
		Object* pointLight = SceneGraph::Instance()->addPointLight(false, Vector3(0.f, 0.f, 50.f), Vector3F(1.0f, 1.0f, 1.0f));
		pointLight->node->SetScale(Vector3(40.f, 40.f, 40.f));
		pointLightCompTest = pointLight->GetComponent<PointLight>();
		pointLightTest = pointLight;

		pointLight = SceneGraph::Instance()->addPointLight(false, Vector3(-1.4f, -1.9f, 9.0f), Vector3F(0.1f, 0.0f, 0.0f));
		pointLight->node->SetScale(Vector3(10.f, 10.f, 10.f));
		//pointLight->materials[0]->SetDiffuseIntensity(5.f);
		pointLight = SceneGraph::Instance()->addPointLight(false, Vector3(0.0f, -1.8f, 4.0f), Vector3F(0.0f, 0.0f, 0.2f));
		pointLight = SceneGraph::Instance()->addPointLight(false, Vector3(0.8f, -1.7f, 6.0f), Vector3F(0.0f, 0.1f, 0.0f));
		//pointLight->materials[0]->SetDiffuseIntensity(1.f);
		
		//Object* plane = SceneGraph::Instance()->addObject("cube", Vector3(0.f, -2.5f, 0.f));
		//body = new RigidBody(plane);
		//plane->AddComponent(body);
		//plane->SetScale(Vector3(25.f, 2.f, 25.f));
		//body->SetMass(DBL_MAX); 
		//body->isKinematic = true;
		//PhysicsManager::Instance()->RegisterRigidBody(body); //manually registered after manually creating rigid body component and assembling the object

		int gridSize = 10;

		for (int i = 0; i < 100; i++)
		{
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-gridSize + increment, gridSize + increment);
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical((gridSize + increment - 2), (gridSize + increment));

			double len = pos.vectLengt();
			Object* sphere = SceneGraph::Instance()->addObject("sphere", pos);
			sphere->materials[0]->SetShininess(20.f);
		}
		currentScene = scene1Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene2()
	{
		Vector3 playerPos = currentCamera->GetInitPos();
		Vector3 camPos = currentCamera->GetPosition2();
		//printf("initPos: %f %f %f\n", playerPos.x, playerPos.y, playerPos.z);
		//printf("camPos: %f %f %f\n", camPos.x, camPos.y, camPos.z);
		int gridSize = 100;
		int x = (int)playerPos.x / gridSize;
		int y = (int)playerPos.y / gridSize;
		int z = (int)playerPos.z / gridSize;

		//if (prevGridPos[0] == x && prevGridPos[1] == y && prevGridPos[2] == z) return;
		//printf("%d %d %d\n", x, y, z);
		prevGridPos[0] = x;
		prevGridPos[1] = y;
		prevGridPos[2] = z;

		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		//SceneGraph::Instance()->SceneObject->AddComponent(bbSystem);
		//printf("%f", coord);
		int xy = ((x + y) * (x + y + 1)) / 2 + y; //unique values for a pair, do it twice to get the unique for three
		int xyz = ((xy + z) * (xy + z + 1)) / 2 + z; //unique values for a pair, do it twice to get the unique for three
		srand(xyz);
		lightsPhysics = false;
		//currentCamera->SetPosition(Vector3(0.f, 10.f, 60.f));

		//Object* instanceObject = SceneGraph::Instance()->addInstanceSystem("icosphere", 100000);
		//instanceObject->GetComponent<InstanceSystem>()->paused = false;

		Object* insideOutCube = SceneGraph::Instance()->addObject("cube_inverted");
		insideOutCube->node->SetScale(Vector3(40,40,40));
		insideOutCube->materials[0]->SetShininess(1.0);
		insideOutCube->materials[0]->AssignTexture(GraphicsStorage::textures["wood"]);
		insideOutCube->materials[0]->tileX = 10;
		insideOutCube->materials[0]->tileY = 10;
		insideOutCube->AddComponent(new RigidBody());

		Object* insideOutCube2 = SceneGraph::Instance()->addObject("cube");
		insideOutCube2->node->SetScale(Vector3(41, 41, 41));
		insideOutCube2->materials[0]->AssignTexture(GraphicsStorage::textures["wood"]);

		PhysicsManager::Instance()->gravity = Vector3();

		pointLightTest = SceneGraph::Instance()->addPointLight(true, Vector3(0,3,0));
		pointLightTest->node->SetScale(Vector3(100, 100, 100));
		pointLightTest->node->SetMovable(true);
		pointLightCompTest = pointLightTest->GetComponent<PointLight>();
		
		//skybox->materials[0]->SetDiffuseIntensity(10);
		
		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight(true);
		directionalLight->node->SetMovable(true);
		directionalLightComp = directionalLight->GetComponent<DirectionalLight>();

		Object* directionalLight2 = SceneGraph::Instance()->addDirectionalLight(true);
		directionalLight2->node->SetMovable(true);
		directionalLightComp2 = directionalLight2->GetComponent<DirectionalLight>();

		Object* directionalLight3 = SceneGraph::Instance()->addDirectionalLight(true);
		directionalLight3->node->SetMovable(true);
		directionalLightComp3 = directionalLight3->GetComponent<DirectionalLight>();

		Object* directionalLight4 = SceneGraph::Instance()->addDirectionalLight(true);
		directionalLight4->node->SetMovable(true);
		directionalLightComp4 = directionalLight4->GetComponent<DirectionalLight>();

		spotLight1 = SceneGraph::Instance()->addSpotLight(true, Vector3(-25.f, 10.f, -50.f));
		spotLight1->materials[0]->SetColor(Vector3F(1.f, 0.7f, 0.8f));
		spotLight1->node->SetMovable(true);
		spotLightComp = spotLight1->GetComponent<SpotLight>();
		spotLightComp->shadowMapBlurActive = false;
		
		//LineSystem* lSystem = DebugDraw::Instance()->lineSystems.front();

		float rS = 1.f;
		
		for (int i = 0; i < 70; i++)
		{
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-gridSize + increment, gridSize + increment);
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical((gridSize + increment + 20) * 100, (gridSize + increment + 22) * 100);
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical(10, (gridSize + increment + 22));
			//pos.x += x*gridSize;//playerPos;
			//pos.y += y*gridSize;//playerPos;
			//pos.z += z*gridSize;//playerPos;

			double len = pos.vectLengt();
			Object* sphere = SceneGraph::Instance()->addObject("icosphere", pos);
			sphere->materials[0]->SetShininess(20.f);
			sphere->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall"]);
			sphere->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall_normal_dtx5"],1);
			//it is better if we can attach the node to the object
			//to do that we simply set the pointer of the node of line to the object we want to follow
			//this will work for get lines but not for the generated ones,
			//generated ones set the position
			//FastLine* line = lSystem->GetLine();

			//line->AttachEndA(&SceneGraph::Instance()->SceneObject->node);
			//line->AttachEndB(&sphere->node);

			//line->colorA = Vector4F(69.f, 0.f, 0.f, 1.f);
			//line->colorB = Vector4F(3.f, 3.f, 3.f, 1.f);

			for (int j = 0; j < 3; j++)
			{
				Vector3 childPos = SceneGraph::Instance()->generateRandomIntervallVectorCubic((int)-len, (int)len) / 4.f;
				double childLen = childPos.vectLengt();
				Object* child = SceneGraph::Instance()->addObjectTo(sphere, "icosphere", childPos);
				child->materials[0]->SetShininess(20.f);
				child->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall"]);
				child->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall_normal_dtx5"], 1);
				//FastLine* line = lSystem->GetLine();

				//line->AttachEndA(&sphere->node);
				//line->AttachEndB(&child->node);

				//line->colorA = Vector4F(1.f, 0.f, 0.f, 1.f);
				//line->colorB = Vector4F(0.f, 1.f, 0.f, 1.f);

				//rS = ((rand() % 35) + 1.f) / 15.f;
				//child->SetScale(Vector3(rS, rS, rS));

				for (int k = 0; k < 5; k++)
				{
					Vector3 childOfChildPos = SceneGraph::Instance()->generateRandomIntervallVectorCubic((int)-childLen, (int)childLen) / 2.f;
					Object* childOfChild = SceneGraph::Instance()->addObjectTo(child, "sphere", childOfChildPos);
					childOfChild->materials[0]->SetShininess(20.f);
					childOfChild->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall"]);
					childOfChild->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall_normal_dtx5"], 1);
					//FastLine* line = lSystem->GetLine();

					//line->AttachEndA(&child->node);
					//line->AttachEndB(&childOfChild->node);

					//line->colorA = Vector4F(6.f, 0.f, 0.f, 1.f);
					//line->colorB = Vector4F(0.f, 0.f, 24.f, 1.f);

					//rS = ((rand() % 45) + 1.f) / 15.f;
					//childOfChild->SetScale(Vector3(rS, rS, rS));
				}

			}
		}
		

		Object* planezx = SceneGraph::Instance()->addObject("fatplane", Vector3(0.f, -10.f, 0.f));
		//SceneGraph::Instance()->unregisterForPicking(planezx);
		planezx->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall"]);
		planezx->materials[0]->AssignTexture(GraphicsStorage::textures["brickwall_normal_dtx5"], 1);
		planezx->materials[0]->tileX = 50;
		planezx->materials[0]->tileY = 50;
		qXCube1 = planezx;
		qXCube1->node->SetMovable(true);
		planezx->node->SetScale(Vector3(10.0, 1.0, 10.0));
		currentScene = scene2Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene3()
	{
		Clear();
		Object* debugObject = SceneGraph::Instance()->addChild();
		DebugDraw::Instance()->Init(debugObject);
		currentCamera->SetPosition(Vector3(0.0, 10.0, 60.0));

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();

		Object* pointLight = SceneGraph::Instance()->addPointLight();
		pointLight->node->SetScale(Vector3(20.0, 20.0, 20.0));
		pointLight->materials[0]->SetColor(Vector3F(1.f, 0.f, 0.f));

		float rS = 1.f;
		for (int i = 0; i < 80; i++)
		{
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-80, 80);
			double len = pos.vectLengt();
			Object* object = SceneGraph::Instance()->addObject("icosphere", pos);
			//object->materials[0]->SetDiffuseIntensity(10.3f);
			RigidBody* body = new RigidBody();
			object->AddComponent(body);
			//PhysicsManager::Instance()->RegisterRigidBody(body);
			rS = (float)(rand() % 5) + 1.f;
			object->node->SetScale(Vector3(rS, rS, rS));
			body->SetCanSleep(false);

			//FastLine* line = DebugDraw::Instance()->lineSystems.front()->GetLine();
			//SceneGraph::Instance()->SceneObject->node->addChild(&line->nodeA);
			//object->node->addChild(&line->nodeB);

			//line->AttachEndA(SceneGraph::Instance()->SceneObject->node);
			//line->AttachEndB(object->node);
			//
			//line->colorA = Vector4F(6.f, 0.f, 0.f, 1.f);
			//line->colorB = Vector4F(3.f, 3.f, 0.f, 1.f);
		}

		for (int i = 0; i < 5000; i++)
		{
			FastPoint* point = DebugDraw::Instance()->pointSystems.front()->GetPoint();
			point->node.localPosition = SceneGraph::Instance()->generateRandomIntervallVectorSpherical(1000, 1100);
		}
		lightsPhysics = true; //it has to update

		PhysicsManager::Instance()->gravity = Vector3();
		currentScene = scene3Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene4()
	{

		Vector3 playerPos = currentCamera->GetInitPos();
		int gridSize = 10;
		int x = (int)playerPos.x / gridSize;
		int y = (int)playerPos.y / gridSize;
		int z = (int)playerPos.z / gridSize;

		//if (prevGridPos[0] == x && prevGridPos[1] == y && prevGridPos[2] == z) return;
		//printf("%d %d %d\n", x, y, z);
		prevGridPos[0] = x;
		prevGridPos[1] = y;
		prevGridPos[2] = z;

		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		int xy = ((x + y) * (x + y + 1)) / 2 + y; //unique values for a pair, do it twice to get the unique for three
		int xyz = ((xy + z) * (xy + z + 1)) / 2 + z; //unique values for a pair, do it twice to get the unique for three		
		srand(xyz);
		lightsPhysics = false;

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();


		LineSystem* lSystem = DebugDraw::Instance()->lineSystems.front();

		float rS = 1.f;


		for (int i = 0; i < 700; i++)
		{
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-gridSize + increment, gridSize + increment); //cube
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical((gridSize + increment - 2), (gridSize + increment)); //sphere
			//pos.x += x*gridSize;//add playerPos;
			//pos.y += y*gridSize;//add playerPos;
			//pos.z += z*gridSize;//add playerPos;

			double len = pos.vectLengt();
			Object* sphere = SceneGraph::Instance()->addObject("icosphere", pos);

			//it is better if we can attach the node to the object
			//to do that we simply set the pointer of the node of line to the object we want to follow
			//this will work for get lines but not for the generated ones,
			//generated ones set the position
			FastLine* line = lSystem->GetLine();

			line->AttachEndA(SceneGraph::Instance()->SceneObject->node);
			line->AttachEndB(sphere->node);

			line->colorA = Vector4F(69.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4F(3.f, 3.f, 0.f, 1.f);

			//rS = ((rand() % 30) + 1.f) / 15.f;
			//sphere->SetScale(Vector3(rS, rS, rS));
			/*
			for (int j = 0; j < 3; j++)
			{
			Vector3 childPos = SceneGraph::Instance()->generateRandomIntervallVectorCubic((int)-len, (int)len) / 4.f;
			float childLen = childPos.vectLengt();
			Object* child = SceneGraph::Instance()->addObjectTo(sphere, "icosphere", childPos);

			FastLine* line = lSystem->GetLine();

			line->AttachEndA(&sphere->node);
			line->AttachEndB(&child->node);

			line->colorA = Vector4(1.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4(0.f, 1.f, 0.f, 1.f);

			//rS = ((rand() % 35) + 1.f) / 15.f;
			//child->SetScale(Vector3(rS, rS, rS));

			for (int k = 0; k < 5; k++)
			{
			Vector3 childOfChildPos = SceneGraph::Instance()->generateRandomIntervallVectorCubic((int)-childLen, (int)childLen) / 2.f;
			Object* childOfChild = SceneGraph::Instance()->addObjectTo(child, "sphere", childOfChildPos);

			FastLine* line = lSystem->GetLine();

			line->AttachEndA(&child->node);
			line->AttachEndB(&childOfChild->node);

			line->colorA = Vector4(6.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4(0.f, 0.f, 24.f, 1.f);

			//rS = ((rand() % 45) + 1.f) / 15.f;
			//childOfChild->SetScale(Vector3(rS, rS, rS));
			}

			}
			*/
		}

		for (int i = 0; i < 500; i++)
		{
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-gridSize + increment, gridSize + increment);
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical((gridSize + increment + 20), (gridSize + increment + 22));
			//pos.x += x*gridSize;//playerPos;
			//pos.y += y*gridSize;//playerPos;
			//pos.z += z*gridSize;//playerPos;

			double len = pos.vectLengt();
			Object* sphere = SceneGraph::Instance()->addObject("icosphere", pos);

			//it is better if we can attach the node to the object
			//to do that we simply set the pointer of the node of line to the object we want to follow
			//this will work for get lines but not for the generated ones,
			//generated ones set the position
			FastLine* line = lSystem->GetLine();

			line->AttachEndA(SceneGraph::Instance()->SceneObject->node);
			line->AttachEndB(sphere->node);

			line->colorA = Vector4F(69.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4F(3.f, 3.f, 3.f, 1.f);
		}
		currentScene = scene4Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene5()
	{

		Vector3 playerPos = currentCamera->GetInitPos();
		int gridSize = 10;
		int x = (int)playerPos.x / gridSize;
		int y = (int)playerPos.y / gridSize;
		int z = (int)playerPos.z / gridSize;

		//if (prevGridPos[0] == x && prevGridPos[1] == y && prevGridPos[2] == z) return;
		//printf("%d %d %d\n", x, y, z);
		prevGridPos[0] = x;
		prevGridPos[1] = y;
		prevGridPos[2] = z;

		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		//printf("%f", coord);
		int xy = ((x + y) * (x + y + 1)) / 2 + y; //unique values for a pair, do it twice to get the unique for three
		int xyz = ((xy + z) * (xy + z + 1)) / 2 + z; //unique values for a pair, do it twice to get the unique for three		
		srand(xyz);
		lightsPhysics = false;
		//currentCamera->SetPosition(Vector3(0.f, 10.f, 60.f));

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();

		LineSystem* lSystem = DebugDraw::Instance()->lineSystems.front();

		float rS = 1.f;


		for (int i = 0; i < 700; i++)
		{
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-gridSize + increment, gridSize + increment);
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical((gridSize + increment - 2), (gridSize + increment));
			//pos.x += x*gridSize;//playerPos;
			//pos.y += y*gridSize;//playerPos;
			//pos.z += z*gridSize;//playerPos;

			double len = pos.vectLengt();
			Object* sphere = SceneGraph::Instance()->addObject("icosphere", pos);

			//it is better if we can attach the node to the object
			//to do that we simply set the pointer of the node of line to the object we want to follow
			//this will work for get lines but not for the generated ones,
			//generated ones set the position
			FastLine* line = lSystem->GetLine();

			line->AttachEndA(SceneGraph::Instance()->SceneObject->node);
			line->AttachEndB(sphere->node);

			line->colorA = Vector4F(69.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4F(3.f, 3.f, 0.f, 1.f);

			//rS = ((rand() % 30) + 1.f) / 15.f;
			//sphere->SetScale(Vector3(rS, rS, rS));
			/*
			for (int j = 0; j < 3; j++)
			{
			Vector3 childPos = SceneGraph::Instance()->generateRandomIntervallVectorCubic((int)-len, (int)len) / 4.f;
			float childLen = childPos.vectLengt();
			Object* child = SceneGraph::Instance()->addObjectTo(sphere, "icosphere", childPos);

			FastLine* line = lSystem->GetLine();

			line->AttachEndA(&sphere->node);
			line->AttachEndB(&child->node);

			line->colorA = Vector4(1.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4(0.f, 1.f, 0.f, 1.f);

			//rS = ((rand() % 35) + 1.f) / 15.f;
			//child->SetScale(Vector3(rS, rS, rS));

			for (int k = 0; k < 5; k++)
			{
			Vector3 childOfChildPos = SceneGraph::Instance()->generateRandomIntervallVectorCubic((int)-childLen, (int)childLen) / 2.f;
			Object* childOfChild = SceneGraph::Instance()->addObjectTo(child, "sphere", childOfChildPos);

			FastLine* line = lSystem->GetLine();

			line->AttachEndA(&child->node);
			line->AttachEndB(&childOfChild->node);

			line->colorA = Vector4(6.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4(0.f, 0.f, 24.f, 1.f);

			//rS = ((rand() % 45) + 1.f) / 15.f;
			//childOfChild->SetScale(Vector3(rS, rS, rS));
			}

			}
			*/
		}

		for (int i = 0; i < 500; i++)
		{
			//Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorCubic(-gridSize + increment, gridSize + increment);
			Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical((gridSize + increment + 20), (gridSize + increment + 22));
			//pos.x += x*gridSize;//playerPos;
			//pos.y += y*gridSize;//playerPos;
			//pos.z += z*gridSize;//playerPos;

			double len = pos.vectLengt();
			Object* sphere = SceneGraph::Instance()->addObject("icosphere", pos);

			//it is better if we can attach the node to the object
			//to do that we simply set the pointer of the node of line to the object we want to follow
			//this will work for get lines but not for the generated ones,
			//generated ones set the position
			FastLine* line = lSystem->GetLine();

			line->AttachEndA(SceneGraph::Instance()->SceneObject->node);
			line->AttachEndB(sphere->node);

			line->colorA = Vector4F(69.f, 0.f, 0.f, 1.f);
			line->colorB = Vector4F(3.f, 3.f, 3.f, 1.f);
		}
		/*
		for (size_t i = 0; i < 500; i++)
		{
		Vector3 pos = SceneGraph::Instance()->generateRandomIntervallVectorSpherical(17, 20);
		SceneGraph::Instance()->addObjectToScene("cube", pos);
		}
		*/

		/*
		Object* plane = SceneGraph::Instance()->addObjectToScene("sphere");
		plane->materials[0]->SetSpecularIntensity(0.5f);
		plane->SetScale(Vector3(10.f, 0.5f, 10.f));
		//plane->materials[0]->SetColor(Vector3(0.f,10.f,10.f));
		plane->materials[0]->SetDiffuseIntensity(100.f);
		this->plane = plane;
		*/
		/*
		ParticleSystem* pSystem = new ParticleSystem(50000, 2000);
		pSystem->SetTexture(GraphicsStorage::textures["particle3"]->handle);
		pSystem->SetLifeTime(5.0f);
		pSystem->SetColor(Vector4F(1.f, 0.f, 0.f, 0.2f));
		pSystem->SetSize(1.f);
		pSystem->SetAdditive(false);
		particleSystems.push_back(pSystem);

		SceneGraph::Instance()->SceneObject->AddComponent(pSystem, true);
		SceneGraph::Instance()->SceneObject->componentsMap["ParticleSystem"] = pSystem;
		SceneGraph::Instance()->SwitchObjectMovableMode(SceneGraph::Instance()->SceneObject, true);
		*/
		currentScene = scene5Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene6()
	{
		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		lightsPhysics = false;
		currentCamera->SetPosition(Vector3(0.f, 20.f, 60.f));

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();
		directionalLight->node->SetMovable(true);
		directionalLightComp = directionalLight->GetComponent<DirectionalLight>();

		Object* plane = SceneGraph::Instance()->addPhysicObject("fatplane", Vector3(0.f, -10.f, 0.f));

		plane->materials[0]->AssignTexture(GraphicsStorage::textures["venus"]);
		//plane->SetScale(Vector3(10.0,1.0,10.0));
		RigidBody* body = plane->GetComponent<RigidBody>();
		body->SetIsKinematic(true); //i might not have to set this to kinematic either, but then phycis will do some extra calculations in the sweep and collision response
		
		for (size_t i = 0; i < 300; i++)
		{
			Object* cube = SceneGraph::Instance()->addPhysicObject("sphere", SceneGraph::Instance()->generateRandomIntervallVectorFlat(-400, 400, SceneGraph::axis::y, 10) / 10.0);
			cube->node->SetMovable(true);
			//cube->node->movable = true; //they update component because it is not kinematic so collisions work, we could set to kinematic it would work as well
		}

		for (size_t i = 0; i < 3; i++)
		{
			Object* cube = SceneGraph::Instance()->addPhysicObject("spaceship", Vector3(0, (i + 1) * 15, 0));
			cube->node->SetMovable(true);
		}
		currentScene = scene6Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene7()
	{
		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		drawBB = true;
		lightsPhysics = false;
		currentCamera->SetPosition(Vector3(0.f, 20.f, 60.f));
		
		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();

		PhysicsManager::Instance()->gravity = Vector3(0.f, -9.f, 0.f);

		SceneGraph::Instance()->addRandomlyPhysicObjects("cube", 600);
		currentScene = scene7Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene8()
	{
		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		drawBB = true;
		lightsPhysics = false;
		currentCamera->SetPosition(Vector3(0.f, 20.f, 60.f));

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();

		PhysicsManager::Instance()->gravity = Vector3();

		SceneGraph::Instance()->addRandomlyPhysicObjects("icosphere", 600);
		currentScene = scene8Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene9()
	{
		//A plank suspended on a static box.	
		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		lightsPhysics = false;
		currentCamera->SetPosition(Vector3(0.f, 20.f, 60.f));

		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight();

	//	Object* plane = SceneGraph::Instance()->addObject("cube", Vector3(0.f, -2.5f, 0.f));
	//	plane->materials[0]->SetShininess(30.f);
	//	plane->materials[0]->SetSpecularIntensity(30.f);
	//	plane->node->SetScale(Vector3(25.f, 2.f, 25.f));
		currentScene = scene9Loaded;
		//Object* instanceObject = SceneGraph::Instance()->addInstanceSystem("icosphere", 100000);
		//instanceObject->GetComponent<InstanceSystem>()->paused = true;

		Object* fastInstanceObject = SceneGraph::Instance()->addFastInstanceSystem("tetra", 10000);
		/*
		Object* pointLight = SceneGraph::Instance()->addPointLight(false, SceneGraph::Instance()->generateRandomIntervallVectorFlat(-20, 20, SceneGraph::y), SceneGraph::Instance()->generateRandomIntervallVectorCubic(0, 6000).toFloat() / 6000.f);
		
		ParticleSystem* pSystem = new ParticleSystem(100000, 1000);
		pSystem->SetTexture(GraphicsStorage::textures[10]->handle);
		pSystem->SetLifeTime(5.0f);
		pSystem->SetColor(Vector4F(1.f, 0.f, 0.f, 0.2f));
		pointLight->AddComponent(pSystem);
		particleSystems.push_back(pSystem);
		*/

		Object* pointLight = SceneGraph::Instance()->addPointLight(false, SceneGraph::Instance()->generateRandomIntervallVectorFlat(-20, 20, SceneGraph::y), SceneGraph::Instance()->generateRandomIntervallVectorCubic(0, 6000).toFloat() / 6000.f);
		Object* sphere = SceneGraph::Instance()->addObject("sphere", pointLight->node->GetLocalPosition());
		sphere->node->SetScale(Vector3(0.1f, 0.1f, 0.1f));
		sphere->materials[0]->shininess = 10.f;

		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::LoadScene0()
	{
		Clear();
		DebugDraw::Instance()->Init(SceneGraph::Instance()->addChild());
		
		Object* directionalLight = SceneGraph::Instance()->addDirectionalLight(false);
		directionalLightComp = directionalLight->GetComponent<DirectionalLight>();
		directionalLight->node->SetMovable(true);
		
		//Object* directionalLight2 = SceneGraph::Instance()->addDirectionalLight(true);
		//directionalLightComp2 = directionalLight2->GetComponent<DirectionalLight>();
		//directionalLight2->node->SetMovable(true);

		//Object* directionalLight3 = SceneGraph::Instance()->addDirectionalLight(true);
		//directionalLightComp3 = directionalLight3->GetComponent<DirectionalLight>();
		//directionalLight3->node->SetMovable(true);

		//Object* directionalLight4 = SceneGraph::Instance()->addDirectionalLight(true);
		//directionalLightComp4 = directionalLight4->GetComponent<DirectionalLight>();
		//directionalLight4->node->SetMovable(true);

		spotLight1 = SceneGraph::Instance()->addSpotLight(true, Vector3(-25.f, 10.f, -50.f));
		spotLight1->node->SetMovable(true);
		spotLightComp = spotLight1->GetComponent<SpotLight>();
		/*
		qXCube1 = SceneGraph::Instance()->addObject("cube");
		qXCube1->materials[0]->SetColor(Vector3F(1,0,0));
		qYCube1 = SceneGraph::Instance()->addObject("cube");
		qYCube1->materials[0]->SetColor(Vector3F(0, 1, 0));
		qZCube1 = SceneGraph::Instance()->addObject("cube");
		qZCube1->materials[0]->SetColor(Vector3F(0, 0, 1));

		qXCube2 = SceneGraph::Instance()->addObject("cube");
		qXCube2->materials[0]->SetColor(Vector3F(1, 0, 0));
		qYCube2 = SceneGraph::Instance()->addObject("cube");
		qYCube2->materials[0]->SetColor(Vector3F(0, 1, 0));
		qZCube2 = SceneGraph::Instance()->addObject("cube");
		qZCube2->materials[0]->SetColor(Vector3F(0, 0, 1));
		*/
		//testCube1 = SceneGraph::Instance()->addObject("cube", Vector3(-3,0,0));
		//testCube1->node->SetOrientation(Quaternion(20, Vector3(1, 0, 0)));
		//testCube1->node->SetScale(Vector3(4, 2, 1));
		//testPyramid = SceneGraph::Instance()->addObjectTo(testCube1, "pyramid", Vector3(5, 0, 0));
		//testPyramid->node->SetOrientation(Quaternion(20, Vector3(0, 1, 0)));
		//testSphere1 = SceneGraph::Instance()->addObject("sphere", Vector3(3,-3,0));


		//Object* planezx = SceneGraph::Instance()->addObject("fatplane", Vector3(0.f, -10.f, 0.f));
		//SceneGraph::Instance()->unregisterForPicking(planezx);
		//planezx->materials[0]->AssignTexture(GraphicsStorage::textures.at(6));
		//planezx->materials[0]->tileX = 50;
		//planezx->materials[0]->tileY = 50;
		//planezx->node->SetScale(Vector3(10.0, 1.0, 10.0));
		//Object* spaceInstances = SceneGraph::Instance()->addInstanceSystem("spaceship", 30000);
		//for (size_t i = 0; i < 10000; i++)
		//{
		//	SceneGraph::Instance()->addChild();
		//}
		/*
		for (size_t i = 0; i < 10; i++)
		{
			for (size_t j = 0; j < 10; j++)
			{
				Object* sphere = SceneGraph::Instance()->addObject("preview_sphere", Vector3(2 * i, 2 * j, 0));
				sphere->materials[0]->AssignTexture(GraphicsStorage::textures.at(20), 0);
				sphere->materials[0]->AssignTexture(GraphicsStorage::textures.at(21), 1);
				sphere->materials[0]->AssignTexture(GraphicsStorage::textures.at(22), 2);
				sphere->materials[0]->shininess = 10 * 1.0 - ((float)i * 0.1);
			}
		}
		
		Object* sphere = SceneGraph::Instance()->addObject("sphere", Vector3(0.f, 10.f, 10.f));
		sphere->materials[0]->AssignTexture(GraphicsStorage::textures.at(17), 0);
		sphere->materials[0]->AssignTexture(GraphicsStorage::textures.at(18), 1);
		sphere->materials[0]->AssignTexture(GraphicsStorage::textures.at(19), 2);
		*/
		Object* cerberusGun = SceneGraph::Instance()->addObject("cerberus_gun", Vector3(0.f, 10.f, 10.f));
		cerberusGun->materials[0]->AssignTexture(GraphicsStorage::textures["Cerberus_A"], 0);
		cerberusGun->materials[0]->AssignTexture(GraphicsStorage::textures["Cerberus_N"], 1);
		cerberusGun->materials[0]->AssignTexture(GraphicsStorage::textures["Cerberus_ORM"], 2);

		currentScene = scene0Loaded;
		SceneGraph::Instance()->InitializeSceneTree();
	}

	void
	PickingApp::FireLightProjectile()
	{
		Object* pointLight = SceneGraph::Instance()->addPointLight(false, currentCamera->GetPosition2()+currentCamera->forward*3.0, Vector3F(1.f, 1.f, 0.f));
		pointLight->materials[0]->SetColor(Vector3F(1.f,0.f,0.f));
		pointLight->node->SetScale(Vector3(15.0, 15.0, 15.0));
		SceneGraph::Instance()->SwitchObjectMovableMode(pointLight, true);

		Object* icos = SceneGraph::Instance()->addObject("icosphere", currentCamera->GetPosition2());
		Object* sphere = SceneGraph::Instance()->addObject("sphere", currentCamera->GetPosition2() + currentCamera->forward*10.0);

		RigidBody* body = new RigidBody();
		pointLight->AddComponent(body, true);
		body->SetCanSleep(false);
		body->ApplyImpulse(currentCamera->forward*3000.0, pointLight->node->GetWorldPosition());

		PhysicsManager::Instance()->RegisterRigidBody(body);

		ParticleSystem* pSystem = new ParticleSystem(500, 170);
		pSystem->SetTexture(GraphicsStorage::textures["particle3"]->handle);
		//pSystem->SetLifeTime(1.f);
		pSystem->SetColor(Vector4F(1.f, 0.f, 0.f, 0.7f));
		pSystem->SetDirection(Vector3F(0.0, 0.0, 0.0));
		//pSystem->SetSize(1.f);
		pSystem->SetAdditive(true);
		pointLight->AddComponent(pSystem, true);

		particleSystems.push_back(pSystem);

		PointSystem* pos = DebugDraw::Instance()->pointSystems.front();
		FastPoint* fpo = pos->GetPoint();
		fpo->color = Vector4F(0.f, 50.f, 50.f, 1.0f);
		fpo->node.localPosition = currentCamera->GetPosition2() + currentCamera->forward*16.0;

		FastLine* line = DebugDraw::Instance()->lineSystems.front()->GetLine();
		//SceneGraph::Instance()->SceneObject->node->addChild(&line->nodeA);
		//object->node->addChild(&line->nodeB);

		//we can attach lines to another objects 
		//line->AttachEndA(icos->node);
		line->SetPositionA(currentCamera->GetPosition2());
		//we can set positions of lines
		line->SetPositionB(currentCamera->GetPosition2() + currentCamera->forward *10.0);

		line->colorA = Vector4F(0.f, 0.f, 5.f, 1.f);
		line->colorB = Vector4F(0.f, 5.f, 0.f, 1.f);

		FastLine* line2 = DebugDraw::Instance()->lineSystems.front()->GetLine();
		//we can attach line to another line if line we are attaching to is using a node of an object
		//line2->AttachEndA(line->nodeA);
		//line2->AttachEndA(sphere->node);
		line2->SetPositionA(currentCamera->GetPosition2() + currentCamera->forward * 10.0);
		line2->SetPositionB(currentCamera->GetPosition2() + currentCamera->forward * 20.0);
		//we can apply some offsets to the attachments
		//line2->SetPositionA(currentCamera->forward *5.0);
		line2->colorA = Vector4F(0.f, 5.f, 0.f, 1.f);
		line2->colorB = Vector4F(5.f, 0.f, 0.f, 1.f);
	}

	void
	PickingApp::Vortex()
	{
		
		for (auto& obj : SceneGraph::Instance()->pointLights)
		{
			if (RigidBody* body = obj->GetComponent<RigidBody>())
			{
				Vector3 dir = obj->node->GetWorldPosition() - Vector3(0.f, -10.f, 0.f);
				body->ApplyImpulse(dir.vectNormalize()*-2000.f, obj->node->GetWorldPosition());
			}
		}
		
		for (auto& obj : SceneGraph::Instance()->renderList)
		{
			if (RigidBody* body = obj->GetComponent<RigidBody>())
			{
				Vector3 dir = obj->node->GetWorldPosition() - Vector3(0.f, -10.f, 0.f);
				body->ApplyImpulse(dir*-1.f, obj->node->GetWorldPosition());
			}
		}
	}

	void
	PickingApp::PIDController()
	{
		int measured_value; //current value
		int setpoint; //target
		int dt; //delta time
		int error; //distance to target? //proportional component calculated from error
		int integral; //integral component
		int derivative; //derivative component
		int output;
		int previous_error = 0;
		int Kp, Ki, Kd; //proprotional gain, integral gain, derivative gain consts
		
		error = setpoint - measured_value; //proportional component
		integral = integral + error * dt;
		derivative = (error - previous_error) / dt;
		output = Kp * error + Ki * integral + Kd * derivative;
		previous_error = error;
		//wait(dt); //should wait until delta time passes before we continue
	}

	void
	PickingApp::BlitDepthToScreenPass()
	{
		FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, 0);
		FBOManager::Instance()->BindFrameBuffer(GL_READ_FRAMEBUFFER, lightAndPostBuffer->handle);
		glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	void
	PickingApp::BlitToScreenPass()
	{
		FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, 0);
		FBOManager::Instance()->BindFrameBuffer(GL_READ_FRAMEBUFFER, lightAndPostBuffer->handle);
		glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR); 
		glBlitFramebuffer(0, 0, windowWidth, windowHeight, 0, 0, windowWidth, windowHeight, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}

	void
	PickingApp::SpawnSomeLights()
	{
		if (SceneGraph::Instance()->pointLights.size() < 500)
		{
			Object* pointLight = SceneGraph::Instance()->addPointLight(false, SceneGraph::Instance()->generateRandomIntervallVectorFlat(-20, 20, SceneGraph::y), SceneGraph::Instance()->generateRandomIntervallVectorCubic(0, 6000).toFloat() / 6000.f);
			Object* sphere = SceneGraph::Instance()->addObject("sphere", pointLight->node->GetLocalPosition());
			sphere->node->SetScale(Vector3(0.1f, 0.1f, 0.1f));
			sphere->materials[0]->shininess = 10.f;
		}
	}

	void
	PickingApp::MouseCallback(double mouseX, double mouseY)
	{
		if (applicationInputEnabled)
		{
			currentCamera->UpdateOrientation(mouseX, mouseY);
			window->SetCursorPos(windowMidX, windowMidY);
		}
	}

	void
	PickingApp::SetUpCamera()
	{
		currentCamera = new Camera(Vector3(0.f, 10.f, 60.f), windowWidth, windowHeight);
		currentCamera->speed = 50;
		currentCamera->Update(Times::Instance()->timeStep);
		window->SetCursorPos(windowMidX, windowMidY);
		CameraManager::Instance()->AddCamera("default", currentCamera);
		CameraManager::Instance()->SetCurrentCamera("default");
		DebugDraw::Instance()->Projection = &currentCamera->ProjectionMatrix;
		DebugDraw::Instance()->View = &currentCamera->ViewMatrix;
	}

	void
	PickingApp::GenerateGUI()
	{
		ImGui::Begin("UI", NULL);
		float start = 0;
		float stop = 360;

		if (ImGui::CollapsingHeader("Tools"))
		{
			if (ImGui::Button("Reload Shaders")) {
				GraphicsManager::ReloadShaders();
			}
			if (ImGui::Button("Select First")) {
				firstObject = lastPickedObject;
			}
			if (ImGui::Button("Select Second")) {
				secondObject = lastPickedObject;
			}
			if (ImGui::Button("Reparent In Place")) {
				firstObject->node->ParentInPlace(secondObject->node);
			}

			if (ImGui::Button("Set Dynamic")) {
				SceneGraph::Instance()->SwitchObjectMovableMode(lastPickedObject, true);
			}
			if (ImGui::Button("Set Static")) {
				SceneGraph::Instance()->SwitchObjectMovableMode(lastPickedObject, false);
			}
			if (firstObject)
			{
				ImGui::Text("Object1: %s ID: %d", typeid(firstObject).name(), firstObject->ID);
			}
			if (secondObject)
			{
				ImGui::Text("Object2: %s ID: %d", typeid(secondObject).name(), secondObject->ID);
			}
			if (lastPickedObject)
			{
				ImGui::Text("LastObject: %s ID: %d", typeid(lastPickedObject).name(), lastPickedObject->ID);
			}
		}
		if (ImGui::CollapsingHeader("Post")) {
			ImGui::Checkbox("Post Effects:", &post);
			ImGui::Checkbox("HDR", &Render::Instance()->hdrEnabled);
			ImGui::Checkbox("Bloom", &Render::Instance()->bloomEnabled);
			ImGui::SliderFloat("Bloom Intensity", &Render::Instance()->bloomIntensity, 0.0f, 5.f);
			ImGui::SliderFloat("Exposure", &Render::Instance()->exposure, 0.0f, 5.0f);
			ImGui::SliderFloat("Gamma", &Render::Instance()->gamma, 0.0f, 5.0f);
			ImGui::SliderFloat("Contrast", &Render::Instance()->contrast, -5.0f, 5.0f);
			ImGui::SliderFloat("Brightness", &Render::Instance()->brightness, -5.0f, 5.0f);
			ImGui::SliderFloat("Bloom Blur Size", &blurBloomSize, 0.0f, 10.0f);
			ImGui::SliderInt("Bloom Level", &bloomLevel, 0, 3);
			ImGui::SliderFloat("Fov", &fov, 0.0f, 180.f);
			ImGui::SliderFloat("Near plane", &near, 0.0f, 5.f);
			ImGui::SliderFloat("Far plane", &far, 0.0f, 5000.f);
		}
		if (ImGui::CollapsingHeader("Stats")) {
			if (ImGui::TreeNode("Modes")) {
				if (Times::Instance()->paused) ImGui::Text("PAUSED");
				if (pausedPhysics) ImGui::Text("PAUSED PHYSICS");
				if (drawBB) ImGui::Text("DRAW BB");
				if (drawLines) ImGui::Text("LINES ON");
				if (drawPoints) ImGui::Text("POINTS ON");
				if (drawParticles) ImGui::Text("PARTICLES ON");
				if (drawMaps) ImGui::Text("MAPS ON");
				ImGui::NewLine();
				ImGui::TreePop();
			}
			ImGui::Text("Objects rendered %d", objectsRendered);
			ImGui::Text("IObjects rendered %d", instancedGeometryDrawn);
			ImGui::Text("BBs rendered %d", DebugDraw::Instance()->boundingBoxesDrawn);
			ImGui::Text("Lights rendered %d", lightsRendered);
			ImGui::Text("Particles rendered %d", particlesRendered);
			ImGui::Text("Update Time %.6f", updateTime);
			ImGui::Text("Update Dynamic Array Time %.6f", SceneGraph::Instance()->updateDynamicArrayTime);
			ImGui::Text("Update Transforms Time %.6f", SceneGraph::Instance()->updateTransformsTime);
			ImGui::Text("Update Bounds Time %.6f", Bounds::updateBoundsTime);
			ImGui::Text("Update MinMax Time %.6f", Bounds::updateMinMaxTime);
			ImGui::Text("Update Components Time %.6f", SceneGraph::Instance()->updateComponentsTime);
			ImGui::Text("Update Dirty Transforms Time %.6f", SceneGraph::Instance()->updateDirtyTransformsTime);
			ImGui::Text("Render Time %.6f", Times::Instance()->deltaTime - updateTime);
			ImGui::Text("FPS %.3f", 1.0 / Times::Instance()->deltaTime);
			ImGui::Text("TimeStep %.6f", 1.0 / Times::Instance()->timeStep);
			ImGui::Text("PickedID %d", pickedID);
			ImGui::Text("PRUNE %.8f", PhysicsManager::Instance()->pruneAndSweepTime);
			ImGui::Text("SAT %.8f", PhysicsManager::Instance()->satTime);
			ImGui::Text("Intersection Test %.8f", PhysicsManager::Instance()->intersectionTestTime);
			ImGui::Text("Generate Contacts %.8f", PhysicsManager::Instance()->generateContactsTime);
			ImGui::Text("Process Contacts %.8f", PhysicsManager::Instance()->processContactTime);
			ImGui::Text("Positional Correction %.8f", PhysicsManager::Instance()->positionalCorrectionTime);
			ImGui::Text("Iterations Count %d", PhysicsManager::Instance()->iterCount);
		}
		if (ImGui::CollapsingHeader("Lights")) {
			if (pointLightTest != nullptr)
			{
				if (ImGui::TreeNode("Point Light"))
				{
					//if (ImGui::TreeNode("Child windows"))
					//{
					ImGui::Checkbox("PointCastShadow", &pointLightCompTest->shadowMapActive);
					ImGui::Checkbox("PointBlurShadow", &pointLightCompTest->shadowMapBlurActive);
					ImGui::SliderFloat("PointFov", &pointLightCompTest->fov, start, stop);
					ImGui::SliderFloat("Point Power", &pointLightCompTest->properties.power, start, stop);
					ImGui::SliderFloat("Point Diffuse", &pointLightCompTest->properties.diffuse, start, stop);
					ImGui::SliderFloat("Point Specular", &pointLightCompTest->properties.specular, start, stop);
					ImGui::ColorEdit3("Point Color", (float*)&pointLightCompTest->properties.color);
					ImGui::SliderFloat("PointSize", &pointScale, 1, 1000);
					ImGui::SliderFloat("PointPosX", &pposX, -100, 100);
					ImGui::SliderFloat("PointPosY", &pposY, -100, 100);
					ImGui::SliderFloat("PointPosZ", &pposZ, -100, 100);
					ImGui::TreePop();
				}
			}
			if (spotLightComp != nullptr)
			{
				if (ImGui::TreeNode("Spot Light"))
				{
					ImGui::NewLine();
					ImGui::Checkbox("SpotCastShadow", &spotLightComp->shadowMapActive);
					ImGui::Checkbox("SpotBlurShadow", &spotLightComp->shadowMapBlurActive);
					ImGui::Text("FPS %.3f", 1.0 / Times::Instance()->deltaTime);
					ImGui::SliderFloat("Spot Power", &spotLightComp->properties.power, start, stop);
					ImGui::SliderFloat("Spot Diffuse", &spotLightComp->properties.diffuse, start, stop);
					ImGui::SliderFloat("Spot Specular", &spotLightComp->properties.specular, start, stop);
					ImGui::ColorEdit3("Spot Color", (float*)&spotLightComp->properties.color);
					ImGui::SliderFloat("Spot X angle", &xAngles, start, stop);
					ImGui::SliderFloat("Spot Y angle", &yAngles, start, stop);
					ImGui::SliderFloat("Spot1PosX", &posX, -100, 100);
					ImGui::SliderFloat("Spot1PosY", &posY, -100, 100);
					ImGui::SliderFloat("Spot1PosZ", &posZ, -100, 100);

					ImGui::SliderFloat("Spot1Const", &spotLightComp->attenuation.Constant, 0, 1);
					ImGui::SliderFloat("Spot1Lin", &spotLightComp->attenuation.Linear, 0, 1);
					ImGui::SliderFloat("Spot1Exp", &spotLightComp->attenuation.Exponential, 0, 1);

					ImGui::SliderFloat("Spot CutOff", &spotLightCutOff, 0, 360);
					ImGui::SliderFloat("Spot OuterCutOff", &spotLightOuterCutOff, 0, 180);

					ImGui::SliderFloat("SpotSizeZ", &spotSZ, 1, 100);
					ImGui::TreePop();
				}
			}
			if (directionalLightComp != nullptr)
			{
				if (ImGui::TreeNode("Directional Light 1"))
				{
					ImGui::NewLine();
					ImGui::Text("DIR1:");
					ImGui::Checkbox("Dir1 CastShadow", &directionalLightComp->shadowMapActive);
					ImGui::Checkbox("Dir1 BlurShadow", &directionalLightComp->shadowMapBlurActive);
					ImGui::SliderFloat("Dir1 Power", &directionalLightComp->properties.power, start, stop);
					ImGui::SliderFloat("Dir1 Diffuse", &directionalLightComp->properties.diffuse, start, stop);
					ImGui::SliderFloat("Dir1 Specular", &directionalLightComp->properties.specular, start, stop);
					ImGui::ColorEdit3("Dir1 Color", (float*)&directionalLightComp->properties.color);
					ImGui::SliderFloat("Dir1 X angle", &xAngled, start, stop);
					ImGui::SliderFloat("Dir1 Y angle", &yAngled, start, stop);
					ImGui::SliderFloat("Dir1 Shadow Blur Size", &directionalLightComp->blurIntensity, 0.0f, 10.0f);
					ImGui::SliderInt("Dir1 Shadow Blur Level", &directionalLightComp->activeBlurLevel, 0, 3);
					ImGui::SliderFloat("Dir1 Ortho Size", &directionalLightComp->radius, 0.0f, 2000.f);
					ImGui::SliderFloat("Dir1 Shadow Fade Range", &directionalLightComp->shadowFadeRange, 0.0f, 50.f);
					ImGui::TreePop();
				}
			}
			if (directionalLightComp2 != nullptr)
			{
				if (ImGui::TreeNode("Directional Light 2"))
				{
					ImGui::NewLine();
					ImGui::Text("DIR2:");
					ImGui::Checkbox("Dir2 CastShadow", &directionalLightComp2->shadowMapActive);
					ImGui::Checkbox("Dir2 BlurShadow", &directionalLightComp2->shadowMapBlurActive);
					ImGui::SliderFloat("Dir2 Power", &directionalLightComp2->properties.power, start, stop);
					ImGui::SliderFloat("Dir2 Diffuse", &directionalLightComp2->properties.diffuse, start, stop);
					ImGui::SliderFloat("Dir2 Specular", &directionalLightComp2->properties.specular, start, stop);
					ImGui::ColorEdit3("Dir2 Color", (float*)&directionalLightComp2->properties.color);
					ImGui::SliderFloat("Dir2 X angle", &xAngled2, start, stop);
					ImGui::SliderFloat("Dir2 Y angle", &yAngled2, start, stop);
					ImGui::SliderFloat("Dir2 Shadow Blur Size", &directionalLightComp2->blurIntensity, 0.0f, 10.0f);
					ImGui::SliderInt("Dir2 Shadow Blur Level", &directionalLightComp2->activeBlurLevel, 0, 3);
					ImGui::SliderFloat("Dir2 2Ortho Size", &directionalLightComp2->radius, 0.0f, 2000.f);
					ImGui::SliderFloat("Dir2 Shadow Fade Range", &directionalLightComp2->shadowFadeRange, 0.0f, 50.f);
					ImGui::TreePop();
				}
			}
			if (directionalLightComp3 != nullptr)
			{
				if (ImGui::TreeNode("Directional Light 3"))
				{
					ImGui::NewLine();
					ImGui::Text("DIR3:");
					ImGui::Checkbox("Dir3 CastShadow", &directionalLightComp3->shadowMapActive);
					ImGui::Checkbox("Dir3 BlurShadow", &directionalLightComp3->shadowMapBlurActive);
					ImGui::SliderFloat("Dir3 X angle", &xAngled3, start, stop);
					ImGui::SliderFloat("Dir3 Y angle", &yAngled3, start, stop);
					ImGui::SliderFloat("Dir3 Shadow Blur Size", &directionalLightComp3->blurIntensity, 0.0f, 10.0f);
					ImGui::SliderInt("Dir3 Shadow Blur Level", &directionalLightComp3->activeBlurLevel, 0, 3);
					ImGui::SliderFloat("Dir3 2Ortho Size", &directionalLightComp3->radius, 0.0f, 2000.f);
					ImGui::SliderFloat("Dir3 Shadow Fade Range", &directionalLightComp3->shadowFadeRange, 0.0f, 50.f);
					ImGui::TreePop();
				}
			}
			if (directionalLightComp4 != nullptr)
			{
				if (ImGui::TreeNode("Directional Light 4"))
				{
					ImGui::NewLine();
					ImGui::Text("DIR4:");
					ImGui::Checkbox("Dir4 CastShadow", &directionalLightComp4->shadowMapActive);
					ImGui::Checkbox("Dir4 BlurShadow", &directionalLightComp4->shadowMapBlurActive);
					ImGui::SliderFloat("Dir4 X angle", &xAngled4, start, stop);
					ImGui::SliderFloat("Dir4 Y angle", &yAngled4, start, stop);
					ImGui::SliderFloat("Dir4 Shadow Blur Size", &directionalLightComp4->blurIntensity, 0.0f, 10.0f);
					ImGui::SliderInt("Dir4 Shadow Blur Level", &directionalLightComp4->activeBlurLevel, 0, 3);
					ImGui::SliderFloat("Dir4 2Ortho Size", &directionalLightComp4->radius, 0.0f, 2000.f);
					ImGui::SliderFloat("Dir4 Shadow Fade Range", &directionalLightComp4->shadowFadeRange, 0.0f, 50.f);
					ImGui::TreePop();
				}
			}
		}
		if(ImGui::CollapsingHeader("Misc")) {
			ImGui::SliderFloat("EnvAngleX", &Render::Instance()->angleX, start, stop);
			ImGui::SliderFloat("EnvAngleY", &Render::Instance()->angleY, start, stop);
			if (currentScene == scene0Loaded || currentScene == scene2Loaded)
			{
				ImGui::NewLine();
				ImGui::SliderFloat("CubeAnglesX", &xCubeAngle, start, stop);
				ImGui::SliderFloat("CubeAnglesY", &yCubeAngle, start, stop);
				ImGui::SliderFloat("CubeAnglesZ", &zCubeAngle, start, stop);

				ImGui::SliderFloat("CubeSpecIntensity", &cubeSpecularIntensity, start, stop);
				ImGui::SliderFloat("CubeSpecShininess", &cubeShininess, start, stop);
			}
		}
		
		ImGui::End();
	}

	void
	PickingApp::SetUpBuffers(int windowWidth, int windowHeight)
	{
		geometryBuffer = FBOManager::Instance()->GenerateFBO();
		worldPosTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT0);
		worldPosTexture->GenerateBindSpecify();
		worldPosTexture->SetClampingToEdge();
		worldPosTexture->SetLinear();
		diffuseTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT1);
		diffuseTexture->GenerateBindSpecify();
		diffuseTexture->SetClampingToEdge();
		diffuseTexture->SetLinear();
		normalTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT2);
		normalTexture->GenerateBindSpecify();
		normalTexture->SetClampingToEdge();
		normalTexture->SetLinear();
		metDiffIntShinSpecIntTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGBA32F, windowWidth, windowHeight, GL_RGBA, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT3);
		metDiffIntShinSpecIntTexture->GenerateBindSpecify();
		metDiffIntShinSpecIntTexture->SetClampingToEdge();
		metDiffIntShinSpecIntTexture->SetLinear();
		pickingTexture = new Texture(GL_TEXTURE_2D, 0, GL_R32UI, windowWidth, windowHeight, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL, GL_COLOR_ATTACHMENT4);
		pickingTexture->GenerateBindSpecify();
		pickingTexture->SetDefaultParameters();
		pickingTexture->SetClampingToEdge();
		pickingTexture->SetLinear();
		depthTexture = new Texture(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, windowWidth, windowHeight, GL_DEPTH_COMPONENT, GL_FLOAT, NULL, GL_DEPTH_STENCIL_ATTACHMENT);
		depthTexture->GenerateBindSpecify();
		depthTexture->SetClampingToEdge();
		depthTexture->SetLinear();
		geometryBuffer->RegisterTexture(worldPosTexture);
		geometryBuffer->RegisterTexture(diffuseTexture);
		geometryBuffer->RegisterTexture(normalTexture);
		geometryBuffer->RegisterTexture(metDiffIntShinSpecIntTexture);
		geometryBuffer->RegisterTexture(pickingTexture);
		geometryBuffer->RegisterTexture(depthTexture);
		geometryBuffer->SpecifyTextures();
		geometryBuffer->CheckAndCleanup();

		lightAndPostBuffer = FBOManager::Instance()->GenerateFBO();
		finalColorTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGBA32F, windowWidth, windowHeight, GL_RGBA, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT0);
		finalColorTexture->GenerateBindSpecify();
		finalColorTexture->SetClampingToEdge();
		finalColorTexture->SetLinear();
		brightLightTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT1);
		brightLightTexture->GenerateBindSpecify();
		brightLightTexture->SetClampingToEdge();
		brightLightTexture->SetLinear();
		lightAndPostBuffer->RegisterTexture(finalColorTexture);
		lightAndPostBuffer->RegisterTexture(brightLightTexture);
		lightAndPostBuffer->SpecifyTextures();
		lightAndPostBuffer->SpecifyTexture(depthTexture);
		lightAndPostBuffer->CheckAndCleanup();

		captureFBO = FBOManager::Instance()->GenerateFBO(false);
		RenderBuffer* rb = new RenderBuffer(6402, 512, 512, 36096);
		rb->GenerateBindSpecify();
		captureFBO->RegisterRenderBuffer(rb);
		captureFBO->SpecifyTextures();
		captureFBO->CheckAndCleanup();

		envCubeMap = new Texture(GL_TEXTURE_CUBE_MAP, 0, 34843, 512, 512, 6407, 5126, NULL, 36064);
		envCubeMap->GenerateBindSpecify();
		envCubeMap->SetDefaultParameters();

		irradianceCubeMap = new Texture(GL_TEXTURE_CUBE_MAP, 0, 34843, 32, 32, 6407, 5126, NULL, 36064);
		irradianceCubeMap->GenerateBindSpecify();
		irradianceCubeMap->SetDefaultParameters();
		
		prefilteredHDRMap = new Texture(GL_TEXTURE_CUBE_MAP, 0, 34843, 128, 128, 6407, 5126, NULL, 36064);
		prefilteredHDRMap->GenerateBindSpecify();
		prefilteredHDRMap->GenerateMipMaps();
		prefilteredHDRMap->SetDefaultParameters();

		brdfTexture = new Texture(GL_TEXTURE_2D, 0, 33327, 512, 512, 6408, 5126, NULL, 36064);
		brdfTexture->GenerateBindSpecify();
		brdfTexture->SetDefaultParameters();

		pbrEnvTextures.push_back(irradianceCubeMap);
		pbrEnvTextures.push_back(prefilteredHDRMap);
		pbrEnvTextures.push_back(brdfTexture);

		Render::Instance()->AddDirectionalShadowMapBuffer(4096, 4096);
		Render::Instance()->AddMultiBlurBuffer(this->windowWidth, this->windowHeight);
		Render::Instance()->AddPingPongBuffer(4096, 4096);
	}

	void
	PickingApp::DrawGeometryMaps(int width, int height)
	{
		GLuint depthPanelShader = GraphicsStorage::shaderIDs["depthPanel"];

		float fHeight = (float)height;
		float fWidth = (float)width;
		int y = (int)(fHeight*0.1f);
		int glWidth = (int)(fWidth *0.1f);
		int glHeight = (int)(fHeight*0.1f);

		Render::Instance()->drawRegion(depthPanelShader, 0, y, glWidth, glHeight, worldPosTexture);
		Render::Instance()->drawRegion(depthPanelShader, 0, 0, glWidth, glHeight, diffuseTexture);
		Render::Instance()->drawRegion(depthPanelShader, glWidth, 0, glWidth, glHeight, normalTexture);
		Render::Instance()->drawRegion(depthPanelShader, glWidth, y, glWidth, glHeight, metDiffIntShinSpecIntTexture);
		Render::Instance()->drawRegion(depthPanelShader, width - glWidth, height - glHeight, glWidth, glHeight, blurredBrightTexture);
		Render::Instance()->drawRegion(depthPanelShader, 0, height - glHeight, glWidth, glHeight, finalColorTexture);
		Render::Instance()->drawRegion(depthPanelShader, width - glWidth, 0, glWidth, glHeight, pickingTexture);
		Render::Instance()->drawRegion(depthPanelShader, 0, height - glHeight - glHeight, glWidth, glHeight, depthTexture);
		Render::Instance()->drawRegion(depthPanelShader, width - glWidth - glWidth, height - glHeight - glHeight, glWidth, glHeight, brdfTexture);
		//if (pointLightTest != drawRegion) DebugDraw::Instance()->DrawMap(width - glWidth, y, glWidth, glHeight, pointLightCompTest->shadowMapTexture->handle, width, height);
		//else if (spotLight1 != nullptr) DebugDraw::Instance()->DrawMap(width - glWidth, y, glWidth, glHeight, spotLightComp->shadowMapTexture->handle, width, height);
		Render::Instance()->drawRegion(depthPanelShader, width - glWidth, y, glWidth, glHeight, Render::Instance()->dirShadowMapBuffer->textures[0]);
	}
} 
