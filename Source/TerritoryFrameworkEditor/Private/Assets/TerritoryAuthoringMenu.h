#pragma once

class UTerritoryPlaceDefinition;

namespace TerritoryAuthoringMenu
{
	void Register();
	void Unregister();
	void SynchronizeFloorsWithDialog(UTerritoryPlaceDefinition* Definition);
}
