/***************************************************************************
# Copyright (c) 2015, NVIDIA CORPORATION. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************/
#include "Session.h"
#include "App.h"

void Session::addTrial(Array<ParameterTable> params) {
	m_remaining.append(params[0].val["trialCount"]);
	m_trialParams.append(params);
}

void Session::nextCondition() {
	Array<int> unrunTrialIdxs;
	for (int i = 0; i < m_remaining.size(); i++) {
		if (m_remaining[i] > 0) {
			unrunTrialIdxs.append(i);
		}
	}
	if (unrunTrialIdxs.size() == 0) return;
	m_currTrialIdx = unrunTrialIdxs[rand() % unrunTrialIdxs.size()];
}

bool Session::isComplete() const{
	bool allTrialsComplete = true;
	for (int remaining : m_remaining) {
		allTrialsComplete = allTrialsComplete && (remaining == 0);
	}
	return allTrialsComplete;
}

bool Session::setupTrialParams(const SessionParameters params)
{
	for (TargetParameters targets : params) {
		for (int i = 0; i < targets.size(); i++) {										// Add the session to each target
			std::string sess = targets[i].str["sessionID"];
			targets[i].add("name", format("%s_%d_%s_%d", sess, (int)targets[i].val["trial_idx"], targets[i].str["id"], i).c_str());
		}
		addTrial(targets);
	}

	// Update the logger w/ these conditions (IS THIS THE RIGHT PLACE TO DO THIS???)
	if (m_config->logger.enable) {
		m_logger->addTargets(m_trialParams);
	}

	// Select the first condition
	nextCondition();
	return true;
}

void Session::onInit(String filename, String userName, String description) {
	// Initialize presentation states
	presentationState = PresentationState::initial;
	m_feedbackMessage = "Click to spawn a target, then use shift on red target to begin.";

	if (m_config->logger.enable) {
		// Setup the logger and create results file
		m_logger = Logger::create();
		m_logger->createResultsFile(filename, userName, description);
	}

	// Check for valid session
	if (m_hasSession) {
		// Iterate over the sessions here and add a config for each
		SessionParameters params = m_app->experimentConfig.getExpConditions(m_config->id);
		setupTrialParams(params);
	}
	else {	// Invalid session, move to displaying message
		presentationState = PresentationState::scoreboard;
	}
}

void Session::randomizePosition(const shared_ptr<TargetEntity>& target) const {
	static const Point3 initialSpawnPos = m_app->activeCamera()->frame().translation;

	ParameterTable tParam = m_trialParams[m_currTrialIdx][target->paramIdx()];
	const bool isWorldSpace = tParam.str["destSpace"] == "world";
	Point3 loc;

	if (isWorldSpace) {
		loc = tParam.bounds.randomInteriorPoint();		// Set a random position in the bounds
		target->resetMotionParams();					// Reset the target motion behavior
	}
	else {
		const float rot_pitch = randSign() * Random::common().uniform(tParam.val["minEccV"], tParam.val["maxEccV"]);
		const float rot_yaw = randSign() * Random::common().uniform(tParam.val["minEccH"], tParam.val["maxEccH"]);
		const CFrame f = CFrame::fromXYZYPRDegrees(initialSpawnPos.x, initialSpawnPos.y, initialSpawnPos.z, rot_yaw, rot_pitch, 0.0f);
		loc = f.pointToWorldSpace(Point3(0, 0, -m_targetDistance));
	}
	target->setFrame(loc);
}

void Session::initTargetAnimation() {
	// initialize target location based on the initial displacement values
	// Not reference: we don't want it to change after the first call.
	//static const Point3 initialSpawnPos = m_app->activeCamera()->frame().translation + Point3(-m_userSpawnDistance, 0.0f, 0.0f);
	const Point3 initialSpawnPos = m_app->activeCamera()->frame().translation;
	CFrame f = CFrame::fromXYZYPRRadians(initialSpawnPos.x, initialSpawnPos.y, initialSpawnPos.z, -initialHeadingRadians, 0.0f, 0.0f);

	// In task state, spawn a test target. Otherwise spawn a target at straight ahead.
	if (presentationState == PresentationState::task) {
		for (int i = 0; i < m_trialParams[m_currTrialIdx].size(); i++) {
			ParameterTable target = m_trialParams[m_currTrialIdx][i];
			float rot_pitch = randSign() * Random::common().uniform(target.val["minEccV"], target.val["maxEccV"]);
			float rot_yaw = randSign() * Random::common().uniform(target.val["minEccH"], target.val["maxEccH"]);
			float visualSize = G3D::Random().common().uniform(target.val["minVisualSize"], target.val["maxVisualSize"]);
			bool isWorldSpace = target.str["destSpace"] == "world";

			CFrame f = CFrame::fromXYZYPRDegrees(initialSpawnPos.x, initialSpawnPos.y, initialSpawnPos.z, rot_yaw- (initialHeadingRadians * 180.0f / (float)pi()), rot_pitch, 0.0f);

			// Check for case w/ destination array
			if (target.val["destCount"] > 0.0) {
				Point3 offset =isWorldSpace ? Point3(0.0, 0.0, 0.0) : f.pointToWorldSpace(Point3(0, 0, -m_targetDistance));
				m_app->spawnDestTarget(
					offset,
					target.destinations,
					visualSize,
					m_config->targetView.healthColors[0],
					String(target.str["id"]),
					i,
					(int)target.val["respawns"],
					String(target.str["name"]),
					target.bools["logTargetTrajectory"]
				);
			}
			// Otherwise check if this is a jumping target
			else if (String(target.str["jumpEnabled"].c_str()) == "true") {
				Point3 offset = isWorldSpace ? target.bounds.randomInteriorPoint() : f.pointToWorldSpace(Point3(0, 0, -m_targetDistance));
				shared_ptr<JumpingEntity> t = m_app->spawnJumpingTarget(
					offset,
					visualSize,
					m_config->targetView.healthColors[0],
					{ target.val["minSpeed"], target.val["maxSpeed"] },
					{ target.val["minMotionChangePeriod"], target.val["maxMotionChangePeriod"] },
					{ target.val["minJumpPeriod"], target.val["maxJumpPeriod"] },
					{ target.val["minDistance"], target.val["maxDistance"] },
					{ target.val["minJumpSpeed"], target.val["maxJumpSpeed"] },
					{ target.val["minGravity"], target.val["maxGravity"] },
					initialSpawnPos,
					m_targetDistance,
					String(target.str["id"]),
					i,
					target.axisLock,
					(int) target.val["respawns"],
					String(target.str["name"]),
					target.bools["logTargetTrajectory"]
				);
				t->setWorldSpace(isWorldSpace);
				if (isWorldSpace) {
					t->setBounds(target.bounds);
				}
			}
			else {
				Point3 offset = isWorldSpace ? target.bounds.randomInteriorPoint() : f.pointToWorldSpace(Point3(0, 0, -m_targetDistance));
				shared_ptr<FlyingEntity> t = m_app->spawnFlyingTarget(
					offset,
					visualSize,
					m_config->targetView.healthColors[0],
					{ target.val["minSpeed"], target.val["maxSpeed"] },
					{ target.val["minMotionChangePeriod"], target.val["maxMotionChangePeriod"] },
					target.bools["upperHemisphereOnly"],
					initialSpawnPos,
					String(target.str["id"]),
					i,
					target.axisLock,
					(int)target.val["respawns"],
					String(target.str["name"]),
					target.bools["logTargetTrajectory"]
				);
				t->setWorldSpace(isWorldSpace);
				if (isWorldSpace) {
					t->setBounds(target.bounds);
				}
			}
		}
	}
	else {
		bool locks[3] = { true };
		// Make sure we reset the target color here (avoid color bugs)
		m_app->spawnFlyingTarget(
			f.pointToWorldSpace(Point3(0, 0, -m_targetDistance)),
			m_config->targetView.refTargetSize,
			m_config->targetView.refTargetColor,
			{ 0.0f, 0.0f },
			{ 1000.0f, 1000.f },
			false,
			initialSpawnPos,
			"reference",
			0,
			locks
		);
	}

	// Reset number of destroyed targets
	m_destroyedTargets = 0;
	// reset click counter
	m_clickCount = 0;
}

void Session::processResponse()
{
	m_taskExecutionTime = m_timer.getTime();
	// Get total target count here
	int totalTargets = 0;
	for (ParameterTable table : m_trialParams[m_currTrialIdx]) {
		if (table.val["respawns"] == -1) {
			totalTargets = MAXINT;		// Ininite spawn case
			break;
		}
		else {
			totalTargets += (int)table.val["respawns"];
		}
	}		
	m_response = totalTargets - m_destroyedTargets; // Number of targets remaining
	recordTrialResponse(); // NOTE: we need record response first before processing it with PsychHelper.
	
	m_remaining[m_currTrialIdx] -= 1;

	String sess = String(m_trialParams[m_currTrialIdx][0].str["sessionID"]);

	// Check for whether all targets have been destroyed
	if (m_response == 0) {
		m_totalRemainingTime += (double(m_config->timing.taskDuration) - m_taskExecutionTime);
		if (m_config->description == "training") {
			m_feedbackMessage = format("%d ms!", (int)(m_taskExecutionTime * 1000));
		}
	}
	else {
		if (m_config->description == "training") {
			m_feedbackMessage = "Failure!";
		}
	}
}

void Session::updatePresentationState()
{
	// This updates presentation state and also deals with data collection when each trial ends.
	PresentationState currentState = presentationState;
	PresentationState newState;
	int remainingTargets = m_app->targetArray.size();
	float stateElapsedTime = m_timer.getTime();

	newState = currentState;

	if (currentState == PresentationState::initial)
	{
		if (!m_app->m_buttonUp)
		{
			//m_feedbackMessage = "";
			newState = PresentationState::feedback;
		}
	}
	else if (currentState == PresentationState::ready)
	{
		if (stateElapsedTime > m_config->timing.readyDuration)
		{
			newState = PresentationState::task;
		}
	}
	else if (currentState == PresentationState::task)
	{
		if ((stateElapsedTime > m_config->timing.taskDuration) || (remainingTargets <= 0) || (m_clickCount == m_config->weapon.maxAmmo))
		{
			m_taskEndTime = Logger::genUniqueTimestamp();
			processResponse();
			m_app->clearTargets(); // clear all remaining targets
			newState = PresentationState::feedback;
		}
	}
	else if (currentState == PresentationState::feedback)
	{
		if ((stateElapsedTime > m_config->timing.feedbackDuration) && (remainingTargets <= 0))
		{
			if (isComplete()) {
				if (m_config->questionArray.size() > 0 && m_currQuestionIdx < m_config->questionArray.size()) {			// Pop up question dialog(s) here if we need to

					if (m_currQuestionIdx == -1){
						m_currQuestionIdx = 0;
						m_app->presentQuestion(m_config->questionArray[m_currQuestionIdx]);
					}
					else if (!m_app->dialog->visible()) {														// Check for whether dialog is closed (otherwise we are waiting for input)
						if (m_app->dialog->complete) {															// Has this dialog box been completed? (or was it closed without an answer?)
							m_config->questionArray[m_currQuestionIdx].result = m_app->dialog->result;			// Store response w/ quesiton
							if (m_config->logger.enable) {
								m_logger->addQuestion(m_config->questionArray[m_currQuestionIdx], m_config->id);	// Log the question and its answer
							}
							m_currQuestionIdx++;																// Present the next question (if there is one)
							if (m_currQuestionIdx < m_config->questionArray.size()) {							// Double check we have a next question before launching the next question
								m_app->presentQuestion(m_config->questionArray[m_currQuestionIdx]);
							}
						}
						else {
							m_app->presentQuestion(m_config->questionArray[m_currQuestionIdx]);					// Relaunch the same dialog (this wasn't completed)
						}	
					}	
				}
				else {
					if (m_config->logger.enable) {
						m_logger->closeResultsFile();															// Close the current results file (if open)
					}
					m_app->markSessComplete(String(m_trialParams[m_currTrialIdx][0].str["sessionID"]));			// Add this session to user's completed sessions
					m_app->updateSessionDropDown();

					int score = int(m_totalRemainingTime);
					m_feedbackMessage = format("Session complete! You scored %d!", score);						// Update the feedback message
					m_currQuestionIdx = -1;
					newState = PresentationState::scoreboard;
				}
			}
			else {
				m_feedbackMessage = "";
				nextCondition();
				newState = PresentationState::ready;
			}
		}
	}
	else if (currentState == PresentationState::scoreboard) {
		//if (stateElapsedTime > m_scoreboardDuration) {
			newState = PresentationState::complete;
			m_app->openUserSettingsWindow();
			if (m_hasSession) {
				m_app->userSaveButtonPress();												// Press the save button for the user...
				Array<String> remaining = m_app		->updateSessionDropDown();
				if (remaining.size() == 0) {
					m_feedbackMessage = "All Sessions Complete!"; // Update the feedback message
					moveOn = false;
				}
				else {
					m_feedbackMessage = "Session Complete!"; // Update the feedback message
					moveOn = true;														// Check for session complete (signal start of next session)
				}
			}
		//}
		//else {
		//	newState = PresentationState::complete;
		//	m_feedbackMessage = "All Sessions Complete!";							// Update the feedback message
		//	moveOn = false;
		//}
	}

	else {
		newState = currentState;
	}

	if (currentState != newState)
	{ // handle state transition.
		m_timer.startTimer();
		if (newState == PresentationState::task) {
			m_taskStartTime = Logger::genUniqueTimestamp();
		}
		presentationState = newState;
		//If we switched to task, call initTargetAnimation to handle new trial
		if ((newState == PresentationState::task) || (newState == PresentationState::feedback)) {
			initTargetAnimation();
		}
	}
}

void Session::onSimulation(RealTime rdt, SimTime sdt, SimTime idt)
{
	// 1. Update presentation state and send task performance to psychophysics library.
	updatePresentationState();

	// 2. Record target trajectories, view direction trajectories, and mouse motion.
	if (presentationState == PresentationState::task)
	{
		accumulateTrajectories();
		accumulateFrameInfo(rdt, sdt, idt);
	}
}

void Session::recordTrialResponse()
{
	if (!m_config->logger.enable) return;		// Skip this if the logger is disabled
	if (m_config->logger.logTrialResponse) {
		String sess = String(m_trialParams[m_currTrialIdx][0].str["sessionID"]);

		// Trials table. Record trial start time, end time, and task completion time.
		Array<String> trialValues = {
			String(std::to_string(m_currTrialIdx)),
			"'" + sess + "'",
			"'" + m_config->description + "'",
			"'" + m_taskStartTime + "'",
			"'" + m_taskEndTime + "'",
			String(std::to_string(m_taskExecutionTime)),
			String(std::to_string(m_response))
		};
		m_logger->recordTrialResponse(trialValues);
	}

	// Target_Trajectory table. Write down the recorded target trajectories.
	if (m_config->logger.logTargetTrajectories) {
		m_logger->recordTargetTrajectory(getTrajectoryForDb());
		m_targetTrajectory.clear();
	}

	// Player_Action table. Write down the recorded player actions.
	if (m_config->logger.logPlayerActions) {
		m_logger->recordPlayerActions(getPlayerActionsForDb());
		m_playerActions.clear();
	}

	// Frame_Info table. Write down all frame info.
	if (m_config->logger.logFrameInfo) {
		m_logger->recordFrameInfo(getFrameInfoForDb());
		m_frameInfo.clear();
	}
}

Array<RowEntry> Session::getTrajectoryForDb() {
	Array<RowEntry> rows;
	for (TargetLocation loc : m_targetTrajectory) {
		Array<String> targetTrajectoryValues = {
			"'" + Logger::formatFileTime(loc.time) + "'",
			"'" + loc.name + "'",
			String(std::to_string(loc.position.x)),
			String(std::to_string(loc.position.y)),
			String(std::to_string(loc.position.z)),
		};
		rows.append(targetTrajectoryValues);
	}
	return rows;
}

Array<RowEntry> Session::getPlayerActionsForDb() {
	Array<RowEntry> rows;
	for (PlayerAction action : m_playerActions) {
		String actionStr = "";
		switch (action.action) {
			case Invalid: actionStr = "invalid"; break;
			case Nontask: actionStr = "non-task"; break;
			case Aim: actionStr = "aim"; break;
			case Miss: actionStr = "miss"; break;
			case Hit: actionStr = "hit"; break;
			case Destroy: actionStr = "destroy"; break;
		}
		Array<String> playerActionValues = {
		"'" + Logger::formatFileTime(action.time) + "'",
		String(std::to_string(action.viewDirection.x)),
		String(std::to_string(action.viewDirection.y)),
		String(std::to_string(action.position.x)),
		String(std::to_string(action.position.y)),
		String(std::to_string(action.position.z)),
		"'" + actionStr + "'",
		"'" + action.targetName + "'",
		};
		rows.append(playerActionValues);
	}
	return rows;
}

Array<RowEntry> Session::getFrameInfoForDb() {
	Array<RowEntry> rows;
	for (FrameInfo info : m_frameInfo) {
		Array<String> frameValues = {
			"'" + Logger::formatFileTime(info.time) + "'",
			//String(std::to_string(info.idt)),
			String(std::to_string(info.sdt))
		};
		rows.append(frameValues);
	}
	return rows;
}

void Session::accumulateTrajectories()
{
	if (m_config->logger.logTargetTrajectories) {
		for (shared_ptr<TargetEntity> target : m_app->targetArray) {
			if (!target->isLogged()) continue;
			// recording target trajectories
			Point3 targetAbsolutePosition = target->frame().translation;
			Point3 initialSpawnPos = m_app->activeCamera()->frame().translation;
			Point3 targetPosition = targetAbsolutePosition - initialSpawnPos;
					   
			//// below for 2D direction calculation (azimuth and elevation)
			//Point3 t = targetPosition.direction();
			//float az = atan2(-t.z, -t.x) * 180 / pif();
			//float el = atan2(t.y, sqrtf(t.x * t.x + t.z * t.z)) * 180 / pif();
			TargetLocation location = TargetLocation(Logger::getFileTime(), target->name(), targetPosition);
			m_targetTrajectory.push_back(location);
		}
	}
	// recording view direction trajectories
	accumulatePlayerAction(PlayerActionType::Aim);
}

void Session::accumulatePlayerAction(PlayerActionType action, String targetName)
{
	if (m_config->logger.logPlayerActions) {
		BEGIN_PROFILER_EVENT("accumulatePlayerAction");
		// recording target trajectories
		Point2 dir = m_app->getViewDirection();
		Point3 loc = m_app->getPlayerLocation();
		PlayerAction pa = PlayerAction(Logger::getFileTime(), dir, loc, action, targetName);
		m_playerActions.push_back(pa);
		END_PROFILER_EVENT();
	}
}

void Session::accumulateFrameInfo(RealTime t, float sdt, float idt) {
	if (m_config->logger.logFrameInfo) {
		m_frameInfo.push_back(FrameInfo(Logger::getFileTime(), sdt));
	}
}

bool Session::canFire() {
	if (isNull(m_config)) return true;
	double timeNow = System::time();
	if ((timeNow - m_lastFireAt) > (m_config->weapon.firePeriod)) {
		m_lastFireAt = timeNow;
		return true;
	}
	else {
		return false;
	}
}

double Session::weaponCooldownPercent() const {
	if (isNull(m_config)) return 1.0;
	if (m_config->weapon.firePeriod == 0.0f) return 1.0;
	return min((System::time() - m_lastFireAt) / m_config->weapon.firePeriod, 1.0);
}

int Session::remainingAmmo() const {
	if (isNull(m_config)) return 100;
	return m_config->weapon.maxAmmo - m_clickCount;
}


float Session::getRemainingTrialTime() {
	if (isNull(m_config)) return 10.0;
	return m_config->timing.taskDuration - m_timer.getTime();
}

float Session::getProgress() {
	if (notNull(m_config)) {
		int completed = 0;
		for (bool c : m_remaining) {
			if (c) completed++;
		}
		return completed / (float)m_config->getTotalTrials();
	}
	return fnan();
}

int Session::getScore() {
	return (int)(10.0 * m_totalRemainingTime);
}

String Session::getFeedbackMessage() {
	return m_feedbackMessage;
}