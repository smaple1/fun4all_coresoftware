#include "TowerDigitization.h"


#include <phool/PHCompositeNode.h>
#include <phool/PHNodeIterator.h>
#include <phool/PHIODataNode.h>

#include <stdio.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include <fun4all/Fun4AllReturnCodes.h>
#include <fun4all/getClass.h>

#include <stdexcept>

using namespace std;

TowerDigitization::TowerDigitization( const std::string& name , const std::string& nameRaw , const std::string& nameDigi ,  int randSeed):
  SubsysReco(name),
  _towersDigi(NULL),
  _nodeNameTowerRaw(nameRaw),
  _nodeNameTowerDigi(nameDigi),
  _randSeed(randSeed),
  _applyPoissonSmearing(false),
  _poissonMean(1),
  _timer( PHTimeServer::get()->insert_new(name) )
{}

int
TowerDigitization::InitRun(PHCompositeNode *topNode)
{
  /* Access DST node */
  PHNodeIterator iter(topNode);

  /* Looking for the DST node */
  PHCompositeNode *dstNode;
  dstNode = dynamic_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
    {
      std::cerr << PHWHERE << "DST Node missing, doing nothing." << std::endl;
      exit(1);
    }

  try
    {
      CreateNodes(topNode);
    }
  catch (std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      exit(1);
    }

  return Fun4AllReturnCodes::EVENT_OK;

}

int
TowerDigitization::process_event(PHCompositeNode *topNode)
{
  if (verbosity)
    {
      std::cout << PHWHERE << "TowerDigitization : Process event entered" << std::endl;
    }

  RawTowerContainer* towersRaw = findNode::getClass<RawTowerContainer>(topNode, _nodeNameTowerRaw.c_str());
  if (!towersRaw)
    {
      std::cerr << PHWHERE << "TowerDigitization : Could not locate input tower node " << _nodeNameTowerRaw << endl;
      exit(1);
    }

  // loop over all towers in the event
  RawTowerContainer::ConstIterator towerit;
  RawTowerContainer::ConstRange towers_begin_end = towersRaw->getTowers();

  RawTowerv2* tower_raw_i = NULL;
  for (towerit = towers_begin_end.first; towerit != towers_begin_end.second; towerit++)
    {
      tower_raw_i= dynamic_cast<RawTowerv2*>( (*towerit).second );

      RawTowerv2* tower_digi_i = (RawTowerv2*)tower_raw_i->clone();

      if ( _applyPoissonSmearing )
	ApplyPoissonSmearing( *tower_digi_i );

      _towersDigi->AddTower(  0, 0, tower_digi_i );
    }

  return Fun4AllReturnCodes::EVENT_OK;
}

int
TowerDigitization::End(PHCompositeNode *topNode)
{
  return Fun4AllReturnCodes::EVENT_OK;
}

void
TowerDigitization::CreateNodes(PHCompositeNode *topNode)
{
  PHNodeIterator iter(topNode);
  PHCompositeNode *runNode = static_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "RUN"));
  if (!runNode)
    {
      std::cerr << PHWHERE << "Run Node missing, doing nothing." << std::endl;
      throw std::runtime_error("Failed to find Run node in TowerDigitization::CreateNodes");
    }

  PHCompositeNode *dstNode = static_cast<PHCompositeNode*>(iter.findFirst("PHCompositeNode", "DST"));
  if (!dstNode)
    {
      std::cerr << PHWHERE << "DST Node missing, doing nothing." << std::endl;
      throw std::runtime_error("Failed to find DST node in TowerDigitization::CreateNodes");
    }

  // Create the output tower node on the tree
  _towersDigi = new RawTowerContainer();

  PHIODataNode<PHObject> *towerNode = new PHIODataNode<PHObject>(_towersDigi, _nodeNameTowerDigi.c_str(), "PHObject");
  dstNode->addNode(towerNode);

  return;
}


void
TowerDigitization::ApplyPoissonSmearing( RawTowerv2& tower )
{
  /* get energy deposited in tower */
  double energy = tower.get_edep();

  /* convert energy in tower to number of photons, use Poisson statistics for smearing */

  //***************************************
  //******* Light Yield Calculation *******
  //***************************************

  const gsl_rng_type * T;
  gsl_rng * r;
	
  /* create a generator chosen by the environment variable GSL_RNG_TYPE */

  gsl_rng_env_setup();

  T = gsl_rng_default;
  r = gsl_rng_alloc (T);
	
  gsl_rng_set(r, _randSeed);
	
  unsigned int smearedYield = gsl_ran_poisson (r, _poissonMean);

  double nPhotons = energy * 1000.0 * smearedYield; //multiply by 1000 to convert GeV to Mev

  /* set tower energy to number of photons */
  tower.set_edep( nPhotons ); //Set the smeared photon light yield to the edep of the smeared tower

  return;
}