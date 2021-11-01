#include <dlprim/shape.hpp>
#include <iostream>

namespace dlprim {
    Shape Shape::unsqueeze(int axis) const
    {
        if(axis < 0)
            axis = axis + size_ + 1;
        DLPRIM_CHECK(0 <= axis && axis<=size_);
        DLPRIM_CHECK(size_+1 <= max_tensor_dim);
        Shape r;
        for(int i=0;i<axis;i++)
            r.shape_[i] = shape_[i];
        r.shape_[axis] = 1;
        for(int i=axis;i<size_;i++)
            r.shape_[i+1] = shape_[i];
        r.size_ = size_ + 1;
        return r;
    }
    Shape Shape::broadcast_strides(Shape const &target) const
    {
        DLPRIM_CHECK(size() <= target.size());
        Shape strides = target;
        size_t stride = 1;
        for(int i=strides.size()-1,pos=size_ - 1;i>=0;i--,pos--) {
            if(pos >= 0) {
                if(shape_[pos] == target[i]) {
                    strides[i] = stride;
                    stride *= target[i];
                }
                else if(shape_[pos] == 1) {
                    strides[i] = 0;
                }
                else {
                    std::ostringstream ss;
                    ss << "Can't broadcast " << *this << " to " << target;
                    throw ValidationError(ss.str());
                }
            }
            else {
                strides[i] = 0;
            }
        }
        return strides;
    }

    std::ostream &operator<<(std::ostream &o,Shape const &s)
    {
        o << '(';
        for(int i=0;i<s.size();i++) {
            if(i > 0)
                o<<',';
            o << s[i];
        }
        o << ')';
        return o;
    }

    /// calculate numpy style broadcast shape
    Shape broadcast(Shape const &ain,Shape const &bin)
    {
        Shape a=ain,b=bin;
        while(a.size() < b.size())
            a=a.unsqueeze(0);
        while(b.size() < a.size())
            b=b.unsqueeze(0);

        Shape r=a;
        for(int i=0;i<a.size();i++) {
            if(a[i] == b[i])
                r[i]=a[i];
            else if(a[i] == 1)
                r[i]=b[i];
            else if(b[i] == 1)
                r[i]=a[i];
            else {
                std::ostringstream ss;
                ss << "Non broadcatable shapes" << ain << " and " << bin;
                throw ValidationError(ss.str());
            }
        }
        return r;
    }

    void shrink_broadcast_ranges(std::vector<Shape> &shapes)
    {
        // Calculate broadcasted shape and strides
        Shape broadcasted = shapes[0];
        for(size_t i=1;i<shapes.size();i++) {
            broadcasted = broadcast(broadcasted,shapes[i]);
        }
        std::vector<Shape> strides(shapes.size());
        for(size_t i=0;i<shapes.size();i++) {
            strides[i] = shapes[i].broadcast_strides(broadcasted);
        }

        /// find dimentions that can be converted to a single
        std::vector<bool> squeezable(broadcasted.size(),true);
        int squeezed=0;
        for(int i=0;i<broadcasted.size()-1;i++) {
            if(!squeezable[i])
                continue;
            for(size_t j=0;j<shapes.size();j++) {
                if(strides[j][i+1]*broadcasted[i+1] != strides[j][i])
                {
                    squeezable[i] = false;
                    break;
                }
            }
            if(squeezable[i])
                squeezed++;
        }
        int final_dim = broadcasted.size() - squeezed;

        for(size_t i=0;i<shapes.size();i++) {
            Shape input=shapes[i];
            while(input.size() < broadcasted.size()) {
                input = input.unsqueeze(0);
            }
            std::array<size_t,max_tensor_dim> values;
            for(int i=0,pos=0;i<final_dim;i++) {
                values[i] = input[pos];
                while(pos + 1 < broadcasted.size() && squeezable[pos]) {
                    values[i]*=input[pos+1];
                    pos++;
                }
                pos++;
            }
            Shape result = Shape::from_range(&values[0],&values[0] + final_dim);
            shapes[i] = result;
        }
    }

} // dlprim