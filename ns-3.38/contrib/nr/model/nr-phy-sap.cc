/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-phy-sap.h"

namespace ns3
{

NrPhySapProvider::~NrPhySapProvider()
{
}

std::vector<NrPhySapProvider::PrachConfig>
NrPhySapProvider::readPrachCSV(std::istream &in)
{
    std::vector<NrPhySapProvider::PrachConfig> table;
    std::string row;
    std::getline (in, row);
    while (!in.eof ())
      {
        std::getline (in, row);
        if (in.bad () || in.fail ())
          {
            break;
          }
        auto fields = readCSVRow (row);
        PrachConfig tmp = PrachConfig( fields[1],stoi (fields[2]),stoi (fields[3]),stoi (fields[5]),stof (fields[6]),stof (fields[7]),stoi (fields[8]));
        // Subframe list, e.g. "1:4:6:9". IoTJ R1 fix: the original code called atoi(&c) on a
        // single char, which reads past the variable (stack-buffer-overflow, found with ASan)
        // and could mis-parse the subframe depending on adjacent stack bytes.
        {
            std::string token;
            for (char c : fields[4] + ":")
            {
                if (c == ':')
                {
                    if (!token.empty())
                    {
                        tmp.m_SfN.emplace_back(static_cast<uint8_t>(std::stoi(token)));
                    }
                    token.clear();
                }
                else if (c >= '0' && c <= '9')
                {
                    token.push_back(c);
                }
            }
        }
        table.emplace_back(tmp);
    }
    return table;
}

 std::vector<std::string>
  NrPhySapProvider::readCSVRow(const std::string &row)
  {
    CSVState state = CSVState::UnquotedField;
    std::vector<std::string> fields{""};
    size_t i = 0; // index of the current field
    for (char c : row)
      {
        switch (state)
          {
          case CSVState::UnquotedField:
            switch (c)
              {
              case ',': // end of field
                fields.push_back ("");
                i++;
                break;
              case '"':
                state = CSVState::QuotedField;
                break;
              default:
                fields[i].push_back (c);
                break;
              }
            break;
          case CSVState::QuotedField:
            switch (c)
              {
              case '"':
                state = CSVState::QuotedQuote;
                break;
              default:
                fields[i].push_back (c);
                break;
              }
            break;
          case CSVState::QuotedQuote:
            switch (c)
              {
              case ',': // , after closing quote
                fields.push_back ("");
                i++;
                state = CSVState::UnquotedField;
                break;
              case '"': // "" -> "
                fields[i].push_back ('"');
                state = CSVState::QuotedField;
                break;
              default: // end of quote
                state = CSVState::UnquotedField;
                break;
              }
            break;
          }
      }
    return fields;
  }

} // namespace ns3
