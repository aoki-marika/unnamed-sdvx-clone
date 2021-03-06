#pragma once

/*
	Inherit from this to disallow copying
*/
class Unique
{
protected:
	Unique(const Unique& rhs) = delete;
	Unique& operator=(const Unique& rhs) = delete;
	Unique() = default;
};
