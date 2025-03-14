#include "SimpleWaterApp.h"
#include <cstring>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "GraphicsManager.h"
#include "GraphicsStorage.h"
#include "Node.h"
#include "Material.h"
#include "Texture.h"
#include "OBJ.h"
#include <fstream>
#include "SceneGraph.h"
#include "ShaderManager.h"
#include <string>
#include "PhysicsManager.h"
#include "Camera.h"
#include "Frustum.h"
#include "Render.h"
#include "CameraManager.h"
#include <chrono>
#include "FBOManager.h"
#include "FrameBuffer.h"
#include "Times.h"
#include "Box.h"
#include "Plane.h"
#include "ImGuiWrapper.h"
#include <imgui.h>


using namespace Display;

namespace SimpleWater
{

	//------------------------------------------------------------------------------
	/**
	*/
	SimpleWaterApp::SimpleWaterApp()
	{
		// empty
	}

	//------------------------------------------------------------------------------
	/**
	*/
	SimpleWaterApp::~SimpleWaterApp()
	{
		// empty
	}

	//------------------------------------------------------------------------------
	/**
	*/
	bool
	SimpleWaterApp::Open()
	{
		App::Open();
		this->window = new Display::Window;

		window->SetKeyPressFunction([this](int32 key, int32 scancode, int32 action, int32 mode)
		{
			KeyCallback(key, scancode, action, mode);
		}); 
		
		window->SetCloseFunction([this]()
		{
			this->running = false;
		});

		// window resize callback
		this->window->SetWindowSizeFunction([this](int width, int height)
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
		this->window->SetMousePressFunction([this](int button, int action, int mods)
		{
			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
			{
				isLeftMouseButtonPressed = true;
			}
			if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
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
				GraphicsManager::ReloadShaders();
			}
			else if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_RELEASE)
			{
			}
		});
		this->window->SetWindowIconifyFunction([this](int iconified){
			if (iconified) minimized = true;
			else minimized = false;
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
	SimpleWaterApp::Run()
	{
		InitGL();

		SetUpBuffers(this->windowWidth, this->windowHeight);

		ImGuiWrapper ImGuiWrapper(window);

		GraphicsManager::LoadAllAssets();

		Render::Instance()->InitializeShderBlockDatas();

		LoadScene1();
		
		Times::Instance()->currentTime = glfwGetTime();
		window->SetCursorMode(GLFW_CURSOR_DISABLED);
		SetUpCamera();

		SceneGraph::Instance()->Update();

		glfwSwapInterval(0); //unlock fps

		// timer query setup
		// use multiple queries to avoid stalling on getting the results
		glGenQueries(querycount, queries);

		while (running)
		{
			glDepthMask(GL_TRUE);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glEnable(GL_DEPTH_TEST);

			glDisable(GL_BLEND);

			this->window->Update();
			if (minimized) continue;
			ImGuiWrapper.NewFrame();

			Times::Instance()->Update(glfwGetTime());

			Monitor(this->window);

			//is cursor window locked
			CameraManager::Instance()->Update(Times::Instance()->deltaTime);
			FrustumManager::Instance()->ExtractPlanes(CameraManager::Instance()->ViewProjection);
			
			SceneGraph::Instance()->Update();

			Render::Instance()->UpdateShaderBlockDatas();

			//for HDR //draw current "to screen" to another texture draw water there too, then draw quad to screen from this texture
			//for Bloom //draw current "to screen" to yet another texture in same shaders
			GenerateGUI(); // <-- (generate) to screen
			FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, frameBuffer->handle);
			DrawReflection(); // <-- to fb texture
			DrawGeoUnderWater(); // <-- to fb texture
			FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, postFrameBuffer->handle);
			DrawSkybox(); // <-- to postfb texture
			Draw(); // <-- to postfb texture
			DrawWater(); // <-- to postfb textures

			// display timer query results from querycount frames before
			if (GL_TRUE == glIsQuery(queries[(current_query + 1) % querycount])) {
				GLuint64 result;
				glGetQueryObjectui64v(queries[(current_query + 1) % querycount], GL_QUERY_RESULT, &result);
				waterShaderTime = result * 1.e-9;
			}
			// advance query counter
			current_query = (current_query + 1) % querycount;

			if (post)
			{
				blurredBrightTexture = Render::Instance()->MultiBlur(brightLightTexture, blurLevel, blurSize, GraphicsStorage::shaderIDs["fastBlur"]); //blur bright color
				FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, 0);
				DrawHDR(blurredBrightTexture); // <-- to screen from hdr and bloom textures
			}
			else
			{
				FBOManager::Instance()->BindFrameBuffer(GL_DRAW_FRAMEBUFFER, 0);
				ShaderManager::Instance()->SetCurrentShader(GraphicsStorage::shaderIDs["quadToScreen"]);
				hdrTexture->ActivateAndBind(0);
				Plane::Instance()->vao.Bind();
				glDrawElements(GL_TRIANGLES, Plane::Instance()->vao.indicesCount, GL_UNSIGNED_SHORT, (void*)0); // mode, count, type, element array buffer offset
			}
			
			
			DrawTextures(windowWidth, windowHeight);
			ImGuiWrapper.Render();
			this->window->SwapBuffers();
		}
		GraphicsStorage::Clear();
		this->window->Close();
	}

	void
	SimpleWaterApp::KeyCallback(int key, int scancode, int action, int mods)
	{
		if (action == GLFW_PRESS)
		{
			if (key == GLFW_KEY_ESCAPE) {
				running = false;
			}
			//this is just used to show hide cursor, mouse steal on/off
			else if (key == GLFW_KEY_LEFT_ALT) {
				if (windowLocked) {
					windowLocked = false;
					window->SetCursorMode(GLFW_CURSOR_NORMAL);
				}
				else {
					windowLocked = true;
					window->SetCursorPos(windowWidth / 2.f, windowHeight / 2.f);
					window->SetCursorMode(GLFW_CURSOR_DISABLED);
				}
			}
			else if (key == GLFW_KEY_1) {
				LoadScene1();
			}
		}
	}

	void
	SimpleWaterApp::Monitor(Display::Window* window)
	{
		currentCamera->holdingForward = (window->GetKey(GLFW_KEY_W) == GLFW_PRESS);
		currentCamera->holdingBackward = (window->GetKey(GLFW_KEY_S) == GLFW_PRESS);
		currentCamera->holdingRight = (window->GetKey(GLFW_KEY_D) == GLFW_PRESS);
		currentCamera->holdingLeft = (window->GetKey(GLFW_KEY_A) == GLFW_PRESS);
		currentCamera->holdingUp = (window->GetKey(GLFW_KEY_SPACE) == GLFW_PRESS);
		currentCamera->holdingDown = (window->GetKey(GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS);

		if (windowLocked)
		{
			double mouseX, mouseY;
			window->GetCursorPos(&mouseX, &mouseY);
			currentCamera->UpdateOrientation(mouseX, mouseY);
			window->SetCursorPos(windowMidX, windowMidY);
		}
		currentCamera->fov = fov;
		currentCamera->near = near;
		currentCamera->far = far;
	}

	void
	SimpleWaterApp::InitGL()
	{
		// grey background
		glClearColor(0.f, 0.f, 0.f, 1.0f);

		// Enable depth test
		glEnable(GL_DEPTH_TEST);
		// Accept fragment if it closer to the camera than the former one
		glDepthFunc(GL_LESS);

		// Cull triangles which normal is not towards the camera
		glEnable(GL_CULL_FACE);

		this->window->GetWindowSize(&this->windowWidth, &this->windowHeight);
		windowMidX = windowWidth / 2.0f;
		windowMidY = windowHeight / 2.0f;

	}

	void
	SimpleWaterApp::Clear()
	{
		SceneGraph::Instance()->Clear();
		PhysicsManager::Instance()->Clear();
		GraphicsStorage::ClearMaterials();
		for (auto& mesh : dynamicMeshes)
		{
			delete mesh;
		}
		dynamicMeshes.clear();
		water = nullptr;
		selectedObject = nullptr;
	}

	void
	SimpleWaterApp::LoadScene1()
	{
		Clear();

		//water object
		water = SceneGraph::Instance()->addChild();

		Material* waterMaterial = new Material();
		waterMaterial->AssignTexture(reflectionBufferTexture, 0);
		waterMaterial->AssignTexture(underWaterBufferTexture, 1);
		waterMaterial->AssignTexture(GraphicsStorage::textures["waternormal2"], 2);
		waterMaterial->AssignTexture(GraphicsStorage::textures["waterdudv2"], 3);
		waterMaterial->AssignTexture(depthTextureBufferTexture, 4);
		water->AssignMaterial(waterMaterial);

		Vao* waterMesh = GenerateWaterMesh(waterSize, waterSize);
		dynamicMeshes.push_back(waterMesh);

		water->vao = waterMesh;
		water->node->SetScale(Vector3(100.f, 1.f, 100.f));
		water->node->SetPosition(Vector3(-50.f, 0.f, 50.f));
		water->node->SetOrientation(Quaternion(90.f, Vector3(0.f, 1.f, 0.f)));

		waterMaterial->SetShininess(300.f);
		waterMaterial->tileX = 6.f;
		waterMaterial->tileY = 6.f;
		GraphicsStorage::materials.push_back(waterMaterial);

		Object* sphere2 = SceneGraph::Instance()->addObject("sphere", Vector3(0.f, -5.f, 0.f));
		sphere2->materials[0]->SetShininess(20.f);

		sphere2 = SceneGraph::Instance()->addObject("sphere", Vector3(0.f, 5.f, 0.f));
		sphere2->node->SetScale(Vector3(4.f, 4.f, 4.f));
		sphere2->materials[0]->SetShininess(200.f);

		Box::Instance()->tex = GraphicsStorage::cubemaps["c2"];

		for (int i = 0; i < 20; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("icosphere", SceneGraph::Instance()->generateRandomIntervallVectorFlat(-20, 20, SceneGraph::axis::y, -5));
			sphere->materials[0]->SetShininess(200.0f);
			//sphere->mat->SetColor(Vector3(2.f,2.f,2.f));
		}

		for (int i = 0; i < 20; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("sphere", SceneGraph::Instance()->generateRandomIntervallVectorFlat(-20, 20, SceneGraph::axis::y, 5));
			sphere->materials[0]->SetShininess(200.0f);
			//sphere->mat->SetColor(Vector3(2.f, 2.f, 2.f));
		}

		for (int i = 0; i < 8; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("cube", Vector3(30.f, i * 2.f, 30.f));
			sphere->materials[0]->SetShininess(200.0f);
			//sphere->mat->SetColor(Vector3(2.f, 2.f, 2.f));
		}

		for (int i = 0; i < 8; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("cube", Vector3(-30.f, i * 2.f, 30.f));
			sphere->materials[0]->SetShininess(200.0f);
		}

		for (int i = 0; i < 8; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("cube", Vector3(-30.f, i * 2.f, -30.f));
			sphere->materials[0]->SetShininess(200.0f);
		}

		for (int i = 0; i < 8; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("cube", Vector3(30.f, i * 2.f, -30.f));
			sphere->materials[0]->SetShininess(200.0f);
		}

		for (int i = 0; i < 12; i++)
		{
			Object* sphere = SceneGraph::Instance()->addObject("cube", Vector3(0.f, -2.f + i * 2.f, 0.f));
			sphere->materials[0]->SetShininess(200.0f);
		}

		Object* plane = SceneGraph::Instance()->addObject("pond", Vector3(0.f, 0.f, 0.f));
		plane->materials[0]->AssignTexture(GraphicsStorage::textures["beachsand"]);
		plane->materials[0]->tileX = 20;
		plane->materials[0]->tileY = 20;
	}

	void
	SimpleWaterApp::SetUpCamera()
	{
		currentCamera = new Camera(Vector3(0.f, 25.f, 66.f), windowWidth, windowHeight);
		currentCamera->Update(Times::Instance()->timeStep);
		window->SetCursorPos(windowMidX, windowMidY+100.0);
		CameraManager::Instance()->AddCamera("default", currentCamera);
		CameraManager::Instance()->SetCurrentCamera("default");
	}

	void
	SimpleWaterApp::GenerateGUI()
	{
		ImGui::Begin("Properties", NULL, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::SliderFloat("Fov", &fov, 0.0f, 180.f);
		ImGui::SliderFloat("Near plane", &near, 0.0f, 5.f);
		ImGui::SliderFloat("Far plane", &far, 0.0f, 2000.f);

		ImGui::NewLine();
		ImGui::Checkbox("Post Effects:", &post);
		ImGui::Checkbox("HDR", &Render::Instance()->hdrEnabled);
		ImGui::Checkbox("Bloom", &Render::Instance()->bloomEnabled);
		ImGui::SliderFloat("Bloom Intensity", &Render::Instance()->bloomIntensity, 0.0f, 5.f);
		ImGui::SliderFloat("Exposure", &Render::Instance()->exposure, 0.0f, 5.0f);
		ImGui::SliderFloat("Gamma", &Render::Instance()->gamma, 0.0f, 5.0f);
		ImGui::SliderFloat("Contrast", &Render::Instance()->contrast, -5.0f, 5.0f);
		ImGui::SliderFloat("Brightness", &Render::Instance()->brightness, -5.0f, 5.0f);
		ImGui::SliderFloat("Blur Size", &blurSize, 0.0f, 10.0f);
		ImGui::SliderInt("Blur Level", &blurLevel, 0, 3);

		ImGui::NewLine();
		ImGui::Text("LIGHTING:");
		ImGui::ColorEdit3("Sun Color", (float*)&light_color);
		ImGui::SliderFloat("Sun Power", &light_power, 0.0f, 10.0f);
		ImGui::SliderFloat("Sun Specular Intensity", &light_specularIntensity, 0.0f, 10.0f);
		ImGui::SliderFloat("Sun Diffuse Intensity", &light_diffuseIntensity, 0.0f, 10.0f);
		ImGui::SliderFloat("Sun Ambient Intensity", &light_ambientIntensity, 0.0f, 1.0);
		ImGui::SliderFloat("Sun Height", &sun_height, 0.0f, 45.0f);
		ImGui::SliderFloat("Rotate Sun", &sun_angle, 0.0f, 360.0f);
		lightInvDir = Matrix3F::rotateAngle(Vector3F(0.f, 1.f, 0.f), sun_angle) * (Vector3F(-25.f, sun_height, 0.f) - Vector3F(0.f, 0.f, 0.f));


		ImGui::NewLine();
		ImGui::Text("SHADING:");
		ImGui::ColorEdit3("Water Color", (float*)&water_color);
		ImGui::SliderFloat("Water Shininess", (float*)&water->materials[0]->shininess, 0.0f, 300.0f);
		ImGui::SliderFloat("Water Speed", &speed_multiplier, 0.0f, 5.0f);
		water_speed = (float)Times::Instance()->currentTime * 0.2f * speed_multiplier;
		ImGui::SliderFloat("Water Tile", &water_tiling, 0.0f, 30.0f);
		water->materials[0]->tileX = water_tiling;
		water->materials[0]->tileY = water_tiling;
		ImGui::SliderFloat("Distortion Strength", &wave_distortion, 0.0f, 10.0f);
		wave_strength = wave_distortion / 100.f;
		ImGui::SliderFloat("Shore Transparency", &max_depth_transparent, 0.0f, 10.0f);
		ImGui::SliderFloat("Water Transparency Depth", &water_transparency_depth, 0.0f, 100.0f);
		ImGui::SliderFloat("Fresnel (Refl/Refr)", &fresnelAdjustment, 0.0f, 5.0f);
		ImGui::SliderFloat("Soften Normals", &soften_normals, 2.0f, 9.0f);


		ImGui::NewLine();
		ImGui::Text("STATS:");
		ImGui::Text("Objects rendered %d", objectsRendered);
		ImGui::Text("FPS %.3f", 1.0 / Times::Instance()->deltaTime);
		ImGui::Text("Water Shader Time %.6f", waterShaderTime);
		ImGui::Text("TimeStep %.3f", 1.0 / Times::Instance()->timeStep);
		ImGui::End();
	}

	void
	SimpleWaterApp::DrawSkybox()
	{
		Vector4F plane = Vector4F(0.f, 1.f, 0.f, 100000.146f);
		Render::Instance()->drawSkyboxWithClipPlane(GraphicsStorage::shaderIDs["skyboxWithClipPlane"], postFrameBuffer, GraphicsStorage::cubemaps["c2"], plane, currentCamera->ViewMatrix);
	}

	void
	SimpleWaterApp::Draw()
	{
		GLuint currentShaderID = GraphicsStorage::shaderIDs["forward"];
		ShaderManager::Instance()->SetCurrentShader(currentShaderID);

		float plane[4] = { 0.0, 1.0, 0.0, 10000.f };
		GLuint planeHandle = glGetUniformLocation(currentShaderID, "plane");
		glUniform4fv(planeHandle, 1, &plane[0]);

		GLuint LightDir = glGetUniformLocation(currentShaderID, "LightInvDirection_worldspace");
		glUniform3fv(LightDir, 1, &lightInvDir.x);

		GLuint liPower = glGetUniformLocation(currentShaderID, "lightPower");
		glUniform1f(liPower, light_power);

		GLuint liColor = glGetUniformLocation(currentShaderID, "lightColor");
		glUniform3fv(liColor, 1, &light_color.x);

		GLuint specularIntensity = glGetUniformLocation(currentShaderID, "specular");
		GLuint diffuseIntensity = glGetUniformLocation(currentShaderID, "diffuse");
		GLuint ambientIntensity = glGetUniformLocation(currentShaderID, "ambient");
		glUniform1f(specularIntensity, light_specularIntensity);
		glUniform1f(diffuseIntensity, light_diffuseIntensity);
		glUniform1f(ambientIntensity, light_ambientIntensity);

		objectsRendered = Render::Instance()->draw(currentShaderID, SceneGraph::Instance()->renderList, CameraManager::Instance()->ViewProjection);
	}

	void
	SimpleWaterApp::DrawReflection()
	{
		
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_FRONT);

		Vector3 pos = currentCamera->GetInitPos();
		pos.y = -pos.y;
		Vector3 dir = currentCamera->forward;
		dir.y = -dir.y;
		Vector3 right = currentCamera->right;
		Matrix4 View = Matrix4::lookAt(
			pos,
			pos + dir,
			right.crossProd(dir)
		);
		View = View * Matrix4::scale(Vector3(1.f, -1.f, 1.f));

		
		Vector4F plane = Vector4F(0.0, 1.0, 0.0, 0.f); //plane normal and height

		Render::Instance()->drawSkyboxWithClipPlane(GraphicsStorage::shaderIDs["skyboxWithClipPlane"], frameBuffer, GraphicsStorage::cubemaps["c2"], plane, View);

		GLuint currentShaderID = GraphicsStorage::shaderIDs["forwardRefRaf"];
		ShaderManager::Instance()->SetCurrentShader(currentShaderID);

		glEnable(GL_CLIP_PLANE0);
		GLuint planeHandle = glGetUniformLocation(currentShaderID, "plane");
		glUniform4fv(planeHandle, 1, &plane[0]);

		GLuint CameraPos = glGetUniformLocation(currentShaderID, "CameraPos");
		Matrix4 viewModel = View.inverse();
		Vector3F camPos = viewModel.getPosition().toFloat();
		glUniform3fv(CameraPos, 1, &camPos.x);

		GLuint LightDir = glGetUniformLocation(currentShaderID, "LightInvDirection_worldspace");
		glUniform3fv(LightDir, 1, &lightInvDir.x);

		GLuint liPower = glGetUniformLocation(currentShaderID, "lightPower");
		glUniform1f(liPower, light_power);

		GLuint liColor = glGetUniformLocation(currentShaderID, "lightColor");
		glUniform3fv(liColor, 1, &light_color.x);

		GLuint specularIntensity = glGetUniformLocation(currentShaderID, "specular");
		GLuint diffuseIntensity = glGetUniformLocation(currentShaderID, "diffuse");
		GLuint ambientIntensity = glGetUniformLocation(currentShaderID, "ambient");
		glUniform1f(specularIntensity, light_specularIntensity);
		glUniform1f(diffuseIntensity, light_diffuseIntensity);
		glUniform1f(ambientIntensity, light_ambientIntensity);

		Matrix4 ViewProjection = View * currentCamera->ProjectionMatrix;

		FrustumManager::Instance()->ExtractPlanes(ViewProjection); //we do frustum culling against reflected frustum planes

		Render::Instance()->draw(currentShaderID, SceneGraph::Instance()->renderList, ViewProjection);

		glCullFace(GL_BACK);
	}

	void
	SimpleWaterApp::DrawGeoUnderWater()
	{
		GLuint currentShaderID = GraphicsStorage::shaderIDs["forwardRefRaf"];
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ShaderManager::Instance()->SetCurrentShader(currentShaderID);

		float plane[4] = { 0.0, -1.0, 0.0, 0.146f }; //plane normal and height
		GLuint planeHandle = glGetUniformLocation(currentShaderID, "plane");
		glUniform4fv(planeHandle, 1, &plane[0]);

		GLuint CameraPos = glGetUniformLocation(currentShaderID, "CameraPos");
		Vector3F camPos = currentCamera->GetPosition2().toFloat();
		glUniform3fv(CameraPos, 1, &camPos.x);

		FrustumManager::Instance()->ExtractPlanes(CameraManager::Instance()->ViewProjection);

		Render::Instance()->draw(currentShaderID, SceneGraph::Instance()->renderList, CameraManager::Instance()->ViewProjection);

		glDisable(GL_CLIP_PLANE0);
	}

	void
	SimpleWaterApp::DrawWater()
	{
		GLuint currentShaderID = GraphicsStorage::shaderIDs["water"];
		ShaderManager::Instance()->SetCurrentShader(currentShaderID);

		water->materials[0]->ActivateAndBind();

		GLuint waterColor = glGetUniformLocation(currentShaderID, "waterColor");
		glUniform3fv(waterColor, 1, &water_color.x);

		GLuint fTime = glGetUniformLocation(currentShaderID, "fTime");
		glUniform1f(fTime, water_speed);

		GLuint LightDir = glGetUniformLocation(currentShaderID, "LightInvDirection_worldspace");
		glUniform3fv(LightDir, 1, &lightInvDir.x);

		GLuint liPower = glGetUniformLocation(currentShaderID, "lightPower");
		glUniform1f(liPower, light_power);

		GLuint liColor = glGetUniformLocation(currentShaderID, "lightColor");
		glUniform3fv(liColor, 1, &light_color.x);

		GLuint waveStr = glGetUniformLocation(currentShaderID, "waveStrength");
		glUniform1f(waveStr, wave_strength);

		GLuint maxDepthTransparent = glGetUniformLocation(currentShaderID, "maxDepthTransparent");
		glUniform1f(maxDepthTransparent, max_depth_transparent);

		GLuint waterRefractionBlend = glGetUniformLocation(currentShaderID, "waterTransparencyDepth");
		glUniform1f(waterRefractionBlend, water_transparency_depth);

		Matrix4 dModel = water->node->TopDownTransform;
		Matrix4F ModelMatrix = dModel.toFloat();

		GLuint ModelMatrixHandle = glGetUniformLocation(currentShaderID, "M");
		glUniformMatrix4fv(ModelMatrixHandle, 1, GL_FALSE, &ModelMatrix[0][0]);

		GLuint shininess = glGetUniformLocation(currentShaderID, "shininess");
		glUniform1f(shininess, water->materials[0]->shininess);

		GLuint specularIntensity = glGetUniformLocation(currentShaderID, "specular");
		GLuint diffuseIntensity = glGetUniformLocation(currentShaderID, "diffuse");
		GLuint ambientIntensity = glGetUniformLocation(currentShaderID, "ambient");
		glUniform1f(specularIntensity, light_specularIntensity);
		glUniform1f(diffuseIntensity, light_diffuseIntensity);
		glUniform1f(ambientIntensity, light_ambientIntensity);

		GLuint tiling = glGetUniformLocation(currentShaderID, "tiling");
		glUniform2f(tiling, water->materials[0]->tileX, water->materials[0]->tileY);

		GLuint fresnel = glGetUniformLocation(currentShaderID, "fresnelAdjustment");
		glUniform1f(fresnel, fresnelAdjustment);

		GLuint watersize = glGetUniformLocation(currentShaderID, "waterSize");
		glUniform1i(watersize, waterSize);

		GLuint softenNormals = glGetUniformLocation(currentShaderID, "softenNormals");
		glUniform1f(softenNormals, soften_normals);

		//bind vao before drawing
		water->vao->Bind();

		// Draw the triangles !
		glBeginQuery(GL_TIME_ELAPSED, queries[current_query]);
		water->vao->Draw();
		glEndQuery(GL_TIME_ELAPSED);
	}

	void
	SimpleWaterApp::SetUpBuffers(int windowWidth, int windowHeight)
	{
		frameBuffer = FBOManager::Instance()->GenerateFBO();
		reflectionBufferTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT0);
		reflectionBufferTexture->GenerateBindSpecify();
		reflectionBufferTexture->SetClampingToEdge();
		reflectionBufferTexture->SetLinear();
		underWaterBufferTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT1);
		underWaterBufferTexture->GenerateBindSpecify();
		underWaterBufferTexture->SetClampingToEdge();
		underWaterBufferTexture->SetLinear();
		depthTextureBufferTexture = new Texture(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, windowWidth, windowHeight, GL_DEPTH_COMPONENT, GL_FLOAT, NULL, GL_DEPTH_ATTACHMENT);
		depthTextureBufferTexture->GenerateBindSpecify();
		depthTextureBufferTexture->SetClampingToEdge();
		depthTextureBufferTexture->SetLinear();

		frameBuffer->RegisterTexture(reflectionBufferTexture);
		frameBuffer->RegisterTexture(underWaterBufferTexture);
		frameBuffer->RegisterTexture(depthTextureBufferTexture);
		frameBuffer->SpecifyTextures();
		frameBuffer->CheckAndCleanup();

		postFrameBuffer = FBOManager::Instance()->GenerateFBO();

		hdrTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT0);
		hdrTexture->GenerateBindSpecify();
		hdrTexture->SetClampingToEdge();
		hdrTexture->SetLinear();
		brightLightTexture = new Texture(GL_TEXTURE_2D, 0, GL_RGB32F, windowWidth, windowHeight, GL_RGB, GL_FLOAT, NULL, GL_COLOR_ATTACHMENT1);
		brightLightTexture->GenerateBindSpecify();
		brightLightTexture->SetClampingToEdge();
		brightLightTexture->SetLinear();
		Texture* postDepthBufferTexture = new Texture(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, windowWidth, windowHeight, GL_DEPTH_COMPONENT, GL_FLOAT, NULL, GL_DEPTH_ATTACHMENT);
		postDepthBufferTexture->GenerateBindSpecify();
		postDepthBufferTexture->SetClampingToEdge();
		postDepthBufferTexture->SetLinear();

		postFrameBuffer->RegisterTexture(hdrTexture);
		postFrameBuffer->RegisterTexture(brightLightTexture);
		postFrameBuffer->RegisterTexture(postDepthBufferTexture);
		postFrameBuffer->SpecifyTextures();
		postFrameBuffer->CheckAndCleanup();

		Render::Instance()->AddMultiBlurBuffer(this->windowWidth, this->windowHeight);
	}

	void
	SimpleWaterApp::DrawHDR(Texture* blurredBrightLightTexture)
	{
		Render::Instance()->drawHDR(GraphicsStorage::shaderIDs["hdrBloom"], hdrTexture, blurredBrightLightTexture);
	}

	void
	SimpleWaterApp::DrawTextures(int width, int height)
	{
		GLuint quadToScreen = GraphicsStorage::shaderIDs["quadToScreen"];

		float fHeight = (float)height;
		float fWidth = (float)width;
		int y = (int)(fHeight*0.1f);
		int glWidth = (int)(fWidth *0.1f);
		int glHeight = (int)(fHeight*0.1f);

		Render::Instance()->drawRegion(quadToScreen, 0, 0, glWidth, glHeight, reflectionBufferTexture);
		Render::Instance()->drawRegion(quadToScreen, width - glWidth, 0, glWidth, glHeight, underWaterBufferTexture);
		Render::Instance()->drawRegion(quadToScreen, width - glWidth, height - glHeight, glWidth, glHeight, blurredBrightTexture);
		Render::Instance()->drawRegion(quadToScreen, 0, height - glHeight, glWidth, glHeight, hdrTexture);
		Render::Instance()->drawRegion(quadToScreen, width - glWidth - glWidth, height - glHeight - glHeight, glWidth, glHeight, depthTextureBufferTexture);
	}

	Vao* SimpleWaterApp::GenerateWaterMesh(int width, int height)
	{
		std::vector<unsigned int> indices;
		std::vector<VertexData> vertexData;
		Vao* waterMesh = new Vao();

		int vertexCount = width * height;
		int squares = (width - 1) * (height - 1);
		int triangles = squares * 2;
		int indicesCount = triangles * 3;

		vertexData.reserve(vertexCount);
		indices.reserve(indicesCount);

		for (int col = 0; col < width; col++)
		{
			for (int row = 0; row < height; row++)
			{
				//since i store the points column wise the next column starts at index = current column * height
				int currentColumn = height * col;
				vertexData.push_back(VertexData((float)col, (float)row));
				//we never do the last row nor last column, we don't do that with borders since they are already a part border faces that were build in previous loop
				if (col == width - 1 || row == height - 1) continue; //this might be more expensive than writing another for loop set just for indices

				//face 1
				//vertex 0 is current column and current row
				//vertex 1 is current column and current row + 1
				//vertex 2 is current column + 1 and current row + 1
				int nextColumn = height * (col + 1); //or currentColumn + height //will use that later
				indices.push_back(currentColumn + row);
				indices.push_back(currentColumn + row + 1);
				indices.push_back(nextColumn + row + 1); //we need to calculate the next column here
				//face 2
				//vertex 0 is current column + 1 and current row + 1 i.e. same as face 1 vertex 2
				//vertex 1 is current column + 1 and current row
				//vertex 2 is current column and current row i.e. same as face 1 vertex 1
				indices.push_back(nextColumn + row + 1);
				indices.push_back(nextColumn + row);
				indices.push_back(currentColumn + row);
			}
		}

		waterMesh->vertexBuffers.reserve(2);
		waterMesh->AddVertexBuffer(&vertexData[0], vertexData.size() * sizeof(VertexData), { {ShaderDataType::Float2, "position"} });
		waterMesh->AddIndexBuffer(&indices[0], indices.size(), IndicesType::UNSIGNED_INT);

		waterMesh->dimensions = Vector3((float)width, 0.f, (float)height);
		waterMesh->center = waterMesh->dimensions / 2.f;
		//waterMesh->obj = obj;

		return waterMesh;
	}
} // namespace
