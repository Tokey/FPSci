import sqlite3
import math

class Trial:
    def __init__(self, conditionId, sessName, sessMode, startTime, endTime, taskExecTime, success):
        self.id = conditionId
        self.sess = sessName
        self.mode = sessMode
        self.startTime = startTime
        self.endTime = endTime
        self.taskExecTime = taskExecTime
        self.success = bool(success)

class Event:
    def __init__(self, time, eventType):
        self.time = time
        self.type = eventType

class Importer:
    """Simple class for importing data from abstract-fps results files"""
    IN_LOG_TIME_FORMAT = '%Y-%m-%d %H:%M:%S.%f'

    def __init__(self, dbName):
        self.db = sqlite3.connect(dbName)

    def queryDb(self, query):
        """Simple method to query the db"""
        c = self.db.cursor()
        c.execute(query)
        return c.fetchall()

    def getTableRows(self, tableName):
        """Get all rows from a particular table"""
        return self.queryDb('SELECT * FROM {0}'.format(tableName))

    def getTrials(self):
        """Get all trials from the trials table"""
        trials = []
        for row in self.getTableRows('Trials'):
            trials.append(Trial(row[0], row[1], row[2], row[3], row[4], row[5], row[6]))
        return trials

    def getTrialIds(self):
        """Get a list of trial ids from the trials table"""
        ids = []
        for trial in self.getTrials(): ids.append(trial.id)
        return ids

    def getTrialById(self, condId):
        """Get a particular trial(s) from the trials table"""
        rows = self.queryDb('SELECT * FROM Trials WHERE [condition_ID] = {0}'.format(condId))
        if(len(rows) > 1):
            out = []
            for row in rows: out.append(Trial(row[0], row[1], row[2], row[3], row[4], row[5], row[6]))
            return out
        elif(len(rows) == 1): 
            row = rows[0]
            return Trial(row[0], row[1], row[2], row[3], row[4], row[5], row[6])
        return None

    def getEvents(self):
        """Get all events from the events table"""
        rows = self.getTableRows('event_log')
        events = []
        for row in rows: events.append(Event(row[0], row[1]))
        return events

    def getTargetPositionsXYZ(self, condId):
        """Get all target positions (for a given condition id) from the Target_Trajectory table"""
        trial = self.getTrialById(condId)
        rows = self.queryDb("SELECT * FROM Target_Trajectory WHERE [time] <= \'" + trial.endTime + "\' AND [time] >= \'" + trial.startTime + "\'")
        positions = []
        for row in rows: positions.append([row[1], row[2], row[3]])
        return positions

    def getTargetPositionsAzimElev(self, condId):
        """Get all target positions (for a given condition id) as azim/elev from Target_Trajectory table"""
        positionsXYZ = self.getTargetPositionsXYZ(condId)
        positions = []
        for [x,y,z] in positionsXYZ:
            r = math.sqrt(x**2+y**2+z**2)
            azim = math.atan(z/x)
            elev = math.asin(y/r)
            positions.append([r, azim, elev])
        return positions
    
    def getPlayerActions(self, condId):
        """Get player actions (for a given condition id) from the Player_Action table"""
        trial = self.getTrialById(condId)
        rows = self.queryDb("SELECT * FROM Player_Action WHERE [time] <= \'" + trial.endTime + "\' AND [time] >= \'" + trial.startTime + "\'")
        actions = []
        for row in rows: actions.append([row[2], row[3], row[1]])
        return actions