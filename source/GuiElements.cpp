#include "GuiElements.h"
#include "App.h"

bool WaypointDisplay::TreeDisplay::onEvent(const GEvent & event) {
	if (!m_visible) {
		return false;
	}
	WaypointDisplay* window = dynamic_cast<WaypointDisplay*>(this->window());
	Vector2 mousePositionDeBumped = event.mousePosition() - Vector2(window->m_scrollPane->horizontalOffset(), window->m_scrollPane->verticalOffset());
	if (event.type == GEventType::MOUSE_BUTTON_DOWN && (m_rect.contains(mousePositionDeBumped))) {
		float y = 0;
		for (int i = 0; i < m_waypoints->size(); ++i) {
			Destination d = (*m_waypoints)[i];
			if (Rect2D::xyxy(m_config.tree_indent, y, float(m_config.tree_display_width_px), y + m_config.tree_height).contains(event.mousePosition())) {
					m_selectedIdx = i;
					return true;
			}
			y += m_config.tree_height;
		}
	}
	return false;
}

WaypointDisplay::TreeDisplay::TreeDisplay(GuiWindow* w, WaypointDisplayConfig config, shared_ptr<Array<Destination>> waypoints) : GuiControl(w) {
	m_config = config;
	m_waypoints = waypoints;
	m_icon = GFont::fromFile(System::findDataFile("icon.fnt"));
}

void WaypointDisplay::TreeDisplay::render(RenderDevice* rd, const shared_ptr<GuiTheme>& theme, bool ancestorsEnabled) const {
	float y = 0;
	#define SHOW_TEXT(x, t) theme->renderLabel(Rect2D::xywh(x + m_config.tree_indent, y, float(m_config.tree_display_width_px), m_config.tree_height), (t), GFont::XALIGN_LEFT, GFont::YALIGN_BOTTOM, true, false);
	for (int i = 0; i < m_waypoints->size(); i++) {
		Destination d = (*m_waypoints)[i];
		// Handle selection here
		if (i == m_selectedIdx) {
			theme->renderSelection(Rect2D::xywh(0, y, float(m_config.tree_display_width_px), m_config.tree_height));
		}
		// Draw the fields
		float pos = m_config.tree_indent;
		SHOW_TEXT(pos, String(std::to_string(i)));
		pos += m_config.idx_column_width_px;
		SHOW_TEXT(pos, String(std::to_string(d.time)));
		pos += m_config.time_column_width_px;
		SHOW_TEXT(pos, d.position.toString());
		y += m_config.tree_height;
	}

	// Make sure that the window is large enough.  Has to be at least the height of the containing window
	// or we aren't guaranteed to have render called again
	y = max(y, m_gui->rect().height()) + 40;
	const_cast<WaypointDisplay::TreeDisplay*>(this)->setHeight(y);
	const_cast<GuiContainer*>(m_parent)->setHeight(y);
}

WaypointDisplay::WaypointDisplay(App* app, const shared_ptr<GuiTheme>& theme, WaypointDisplayConfig config, shared_ptr<Array<Destination>> waypoints) :
	GuiWindow("Waypoint Manager",
		theme,
		Rect2D::xywh(0, 0, (float)config.tree_display_width_px + 10, (float)config.tree_display_height_px+10),
		GuiTheme::NORMAL_WINDOW_STYLE,
		GuiWindow::HIDE_ON_CLOSE)
{
	// Store the app pointer 
	m_app = app;

	// Create a pane
	GuiPane* pane = GuiWindow::pane();

	// Basic control
	pane->beginRow(); {
		pane->addButton("Drop waypoint", m_app, &App::dropWaypoint);
		auto c = pane->addNumberBox("Delay", &m_app->waypointDelay, "s");
		c->setCaptionWidth(40.0f);
		c->setWidth(120.0f);
		pane->addNumberBox("Height Offset", &m_app->waypointVertOffset, "m")->setWidth(150.0f);
	}; pane->endRow();
	// Removing waypoints
	pane->beginRow(); {
		pane->addButton("Remove waypoint", m_app, &App::removeHighlighted);
		pane->addButton("Remove last", m_app, &App::removeLastWaypoint);
		pane->addButton("Clear all", m_app, &App::clearWaypoints);
	} pane->endRow();
	// File control
	pane->beginRow(); {
		pane->addButton("Load", m_app, &App::loadWaypoints);
		pane->addButton("Save", m_app, &App::exportWaypoints);
		auto t = pane->addTextBox("Filename", &m_app->waypointFile);
		t->setCaptionWidth(60.0f);
		t->setWidth(180.0f);
	} pane->endRow();
	// Preview
	pane->beginRow(); {
		pane->addButton("Preview", m_app, &App::previewWaypoints);
		pane->addButton("Stop Preview", m_app, &App::stopPreview);
	} pane->endRow();
	// Recording
	pane->beginRow(); {
		pane->addCheckBox("Record motion", &m_app->recordMotion);
		pane->addDropDownList("Record Mode",
			Array<GuiText> {GuiText("Fixed Distance"), GuiText("Fixed Time")},
			&m_app->recordMode);
	} pane->endRow();
	pane->beginRow();{
		auto c = pane->addNumberBox("Interval", &m_app->recordInterval);
		c->setCaptionWidth(50.0f);
		c->setWidth(120.0f);
		c = pane->addNumberBox("Time Scale", &m_app->recordTimeScaling, "x");
		c->setCaptionWidth(80.0f);
		c->setWidth(150.0f);
	} pane->endRow();


	// Setup the row labels
	GuiLabel* a = pane->addLabel("Index"); a->setWidth(config.idx_column_width_px + config.tree_indent);
	GuiLabel* b = pane->addLabel("Time"); b->setWidth(config.time_column_width_px + config.tree_indent); b->moveRightOf(a); a = b;
	b = pane->addLabel("Position"); b->setWidth(config.xyz_column_width_px + config.tree_indent); b->moveRightOf(a); a = b;

	// Create the tree display
	m_treeDisplay = new TreeDisplay(this, config, waypoints);
	m_treeDisplay->moveBy(0, -5);		// Dunno why this happens...
	m_treeDisplay->setSize((float)config.tree_display_width_px, (float)config.tree_display_height_px);

	// Create the scroll pane
	m_scrollPane = pane->addScrollPane(true, true);
	m_scrollPane->setSize((float)m_treeDisplay->rect().width()+10, (float)config.tree_display_height_px+10);
	m_scrollPane->viewPane()->addCustom(m_treeDisplay);
	pack();

	// Move to right location
	moveTo(Vector2(app->window()->width() - rect().width() - 10, 50));
}

shared_ptr<WaypointDisplay> WaypointDisplay::create(App* app, const shared_ptr<GuiTheme>& theme, WaypointDisplayConfig config, shared_ptr<Array<Destination>> waypoints) {
	return createShared<WaypointDisplay>(app, theme, config, waypoints);
}

void WaypointDisplay::setManager(WidgetManager *manager) {
	GuiWindow::setManager(manager);
	if (manager) {
		// Move to the upper right
		///float osWindowWidth = (float)manager->window()->width();
		///setRect(Rect2D::xywh(osWindowWidth - rect().width(), 40, rect().width(), rect().height()));
	}
}

PlayerControls::PlayerControls(FpsConfig config, std::function<void()> exportCallback,
	const shared_ptr<GuiTheme>& theme, float width, float height) :
	GuiWindow("Player Controls", theme, Rect2D::xywh(5, 5, width, height), GuiTheme::NORMAL_WINDOW_STYLE, GuiWindow::HIDE_ON_CLOSE)
{
	// Create the GUI pane
	GuiPane* pane = GuiWindow::pane();

	// Get default values...
	playerHeight = config.playerHeight;
	crouchHeight = config.crouchHeight;
	moveRate = config.moveRate;

	pane->beginRow(); {
		auto  c = pane->addNumberBox("Player Height", &playerHeight, "m", GuiTheme::LINEAR_SLIDER, 0.2f, 3.0f);
		c->setCaptionWidth(width / 2);
		c->setWidth(width*0.95f);
	} pane->endRow();
	pane->beginRow(); {
		auto c = pane->addNumberBox("Player Crouch Height", &crouchHeight, "m", GuiTheme::LINEAR_SLIDER, 0.2f, 3.0f);
		c->setCaptionWidth(width / 2);
		c->setWidth(width*0.95f);
	} pane->endRow();
	pane->beginRow(); {
		auto c = pane->addNumberBox("Move Rate", &moveRate, "m/s", GuiTheme::LINEAR_SLIDER, 0.0f, 30.0f);
		c->setCaptionWidth(width / 2);
		c->setWidth(width*0.95f);
	}pane->endRow();
	pane->beginRow(); {
		pane->addButton("Set Start Position", exportCallback);
	} pane->endRow();

	pack();
	moveTo(Vector2(0, 480));
}

shared_ptr<PlayerControls> PlayerControls::create(FpsConfig config, std::function<void()> exportCallback, const shared_ptr<GuiTheme>& theme, float width, float height) {
	return createShared<PlayerControls>(config, exportCallback, theme, width, height);
}

RenderControls::RenderControls(FpsConfig config, bool drawFps, bool turbo, int reticleIndex, int numReticles, float b,
	const shared_ptr<GuiTheme>& theme, float width, float height) :
	GuiWindow("Render Controls", theme, Rect2D::xywh(5,5,width,height), GuiTheme::NORMAL_WINDOW_STYLE, GuiWindow::HIDE_ON_CLOSE)
{
	// Create the GUI pane
	GuiPane* pane = GuiWindow::pane();

	showBullets = config.weapon.renderBullets;
	showWeapon = config.weapon.renderModel;
	showHud = config.showHUD;
	showFps = drawFps;
	turboMode = turbo;
	frameRate = config.frameRate;
	frameDelay = config.frameDelay;
	reticleIdx = reticleIndex;
	brightness = b;

	pane->beginRow(); {
		pane->addCheckBox("Show Bullets", &showBullets);
		pane->addCheckBox("Show Weapon", &showWeapon);
		pane->addCheckBox("Show HUD", &showHud);

	} pane->endRow();
	pane->beginRow(); {
		pane->addCheckBox("Show FPS", &showFps);
		pane->addCheckBox("Turbo mode", &turboMode);
	}pane->endRow();
	pane->beginRow(); {
		auto c = pane->addNumberBox("Framerate", &frameRate);//Pointer<float>(
		c->setWidth(width*0.95f);
	} pane->endRow();
	pane->beginRow(); {
		auto c = pane->addNumberBox("Display Lag", &frameDelay, "f", GuiTheme::LINEAR_SLIDER, 0, 60);
		c->setWidth(width*0.95f);
	}pane->endRow();
	pane->beginRow(); {
		auto c = pane->addNumberBox("Reticle", &reticleIdx, "", GuiTheme::LINEAR_SLIDER, 0, numReticles, 1);
		c->setWidth(width*0.95f);
	}
	pane->beginRow();{
		auto c = pane->addNumberBox("Brightness", &brightness, "x", GuiTheme::LOG_SLIDER, 0.01f, 2.0f);
		c->setWidth(width*0.95f);
	} pane->endRow();

	pack();
	moveTo(Vector2(0, 300));
}

shared_ptr<RenderControls> RenderControls::create(FpsConfig config, bool drawFps, bool turbo, int reticleIdx, int numReticles, float brightness, 
	const shared_ptr<GuiTheme>& theme, float width, float height) {
	return createShared<RenderControls>(config, drawFps, turbo, reticleIdx, numReticles, brightness, theme, width, height);
}