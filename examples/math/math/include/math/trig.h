// Copyright 2022 The Dredd Project Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MATH_TRIG_H
#define MATH_TRIG_H

namespace math {

double SinN(const double &x, const int &n);
double CosN(const double &x, const int &n);
double TanN(const double &x, const int &n);
double SecN(const double &x, const int &n);
double CosecN(const double &x, const int &n);
double CotN(const double &x, const int &n);

} // namespace math

#endif // MATH_TRIG_H
