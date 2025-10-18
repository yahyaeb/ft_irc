/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: yel-bouk <yel-bouk@student.42nice.fr>      +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/18 18:05:35 by yel-bouk          #+#    #+#             */
/*   Updated: 2025/10/18 18:10:44 by yel-bouk         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../resources/Irc.hpp"

std::string upper(const std::string &s)
{
	std::string r(s);
	for (size_t i = 0; i < r.size(); ++i)
		r[i] = std::toupper(static_cast<unsigned char>(r[i]));
	return r;
}

std::string trim(const std::string& s)
{
	std::string::size_type a = s.find_first_not_of(" \t\r\n");
	if (a == std::string::npos) return "";
	std::string::size_type b = s.find_last_not_of(" \t\r\n");
	return s.substr(a, b - a + 1);
}

std::vector<std::string> splitWords(const std::string& s)
{
	std::vector<std::string> out;
	std::string cur;
	for (size_t i = 0; i < s.size(); ++i) {
		if (s[i] == ' ') {
			if (!cur.empty()) { out.push_back(cur); cur.clear(); }
			continue;
		}
		if (s[i] == ':' && (i == 0 || s[i-1] == ' ')) {
			out.push_back(s.substr(i+1)); // trailing part
			return out;
		}
		cur.push_back(s[i]);
	}
	if (!cur.empty()) out.push_back(cur);
	return out;
}
