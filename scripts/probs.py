import sys, os

# frequency counters of SELECTED action
sIdle, sAttack = 0, 0
moveKeys = [
'0000', '0001', '0010', '0011',
'0100', '0101', '0110', '0111',
'1000', '1001', '1010', '1011',
'1100', '1101', '1110', '1111']
sMove = {x: 0 for x in moveKeys}

# frequency counters of slected action WHEN optiens where...
optionsKeys = ['Idle', 'Attack',
'Move0000', 'Move0001', 'Move0010', 'Move0011',
'Move0100', 'Move0101', 'Move0110', 'Move0111',
'Move1000', 'Move1001', 'Move1010', 'Move1011',
'Move1100', 'Move1101', 'Move1110', 'Move1111']

totalCount = {x: 0 for x in optionsKeys}

sIdleWhen, sAttackWhen = {x: 0 for x in optionsKeys}, {x: 0 for x in optionsKeys}
sMoveWhen = {x: 0 for x in moveKeys}
for key in moveKeys:
  sMoveWhen[key] = {x: 0 for x in optionsKeys}



def printPorbability(name, action, states):
  print "======================================"
  print "Prob of {} when one of the states is".format(name)
  print "======================================"
  for key in optionsKeys:
    if states[key] == 0:
      print "- {} was 0".format(key)
    else:
      print "- {} {} ({} of {})".format(key, states[key] / float(action), states[key], action)


def printCvariable(name, action, states):
  print "  {{ probName::{}, {{".format(name)
  
  for key in optionsKeys[:-1]:
    if states[key] == 0:
      print "    {{ probName::{}, 0.0 }},".format(key)
    else:
      print "    {{ probName::{}, {} }},".format(key, states[key] / float(action))

  key = optionsKeys[-1]
  if states[key] == 0:
    print "    {{ probName::{}, 0.0 }}".format(key)
  else:
    print "    {{ probName::{}, {} }}".format(key, states[key] / float(action))



# [hasFriend, hasEnemy, towardsFriend, towardsEnemy]
def getRegionProperties(regID, regList):
  # print "Looking features of region {} in {}".format(regID, regList)
  for region in regList:
    regProp = region.split(':')
    if regProp[0] == regID:
      # print regProp[1].replace(',', '')
      return regProp[1].replace(',', '')
      break
  print "Error, features of reg {} not found in {}".format(regID, regList)
  return '000'

for fileName in os.listdir(os.getcwd()):
    if fileName.endswith(".asd"): 
      # parse one file
      print fileName
      with open(fileName) as file: 
        for line in file:
          elements = line.strip().split('#')
          currentAction = elements[0].split(',')
          possibleActions = elements[1].split(',')
         
          # map possible move action to their properties
          possibleActions2 = ['Idle'] # idle is always an option
          movePropertiesSeen = []
          skip = False
          for action in possibleActions:
            actionTuple = action.split(':')
            if "ATTACK" == actionTuple[0]:
              possibleActions2.append('Attack')
            elif "MOVE" == actionTuple[0]:
              # search region properties
              properties = getRegionProperties(actionTuple[1], elements[2:])
              if properties not in movePropertiesSeen: # we only want moves with unique properties
                possibleActions2.append('Move'+properties)
                movePropertiesSeen.append(properties)
            else: 
              print "[ERROR] Unknwon possible action [{}] skipping line {}".format(actionTuple[0], line.strip())
              skip = True

          if skip: continue

          for action in possibleActions2:
            totalCount[action] += 1

          if currentAction[3] == "Idle":
            sIdle += 1
            for posAction in possibleActions2: sIdleWhen[posAction] += 1
          elif currentAction[3] == "Attack":
            sAttack += 1
            for posAction in possibleActions2: sAttackWhen[posAction] += 1
          elif currentAction[3] == "Move":
            properties = getRegionProperties(currentAction[4], elements[2:])
            sMove[properties] +=1
            for posAction in possibleActions2: sMoveWhen[properties][posAction] += 1
          else: print "Unknwon selected action {}".format(currentAction[3])


# Prob_IDLE = sIdle / float(totalCount['Idle'])
# print "P(IDLE) = {} ({} of {})".format(Prob_IDLE, sIdle, totalCount['Idle'])

# Prob_ATTACK = sAttack / float(totalCount['Attack'])
# print "P(ATTACK) = {} ({} of {})".format(Prob_ATTACK, sAttack, totalCount['Attack'])

# for moveType in moveKeys:
#   print "P(Move{}) = {} ({} of {})".format(moveType, sMove[moveType] / float(totalCount['Move'+moveType]), sMove[moveType], totalCount['Move'+moveType])


print "std::map<uint8_t, double> basicProb = {"
print "  {{ probName::{}, {} }},".format('Idle', sIdle / float(totalCount['Idle']))
print "  {{ probName::{}, {} }},".format('Attack', sAttack / float(totalCount['Attack']))
for moveType in moveKeys[:-1]:
  print "  {{ probName::{}, {} }},".format('Move'+moveType, sMove[moveType] / float(totalCount['Move'+moveType]))
moveType = moveKeys[-1]
print "  {{ probName::{}, {} }}".format('Move'+moveType, sMove[moveType] / float(totalCount['Move'+moveType]))
print "};"

print ""

print "std::map<uint8_t, std::map<uint8_t, double> > priorProb = {"
printCvariable("Idle", sIdle, sIdleWhen)
print "  } },"
printCvariable("Attack", sAttack, sAttackWhen)
for moveType in moveKeys:
  print "  } },"
  printCvariable('Move'+moveType, sMove[moveType], sMoveWhen[moveType])
print "  } }"
print "};"






