#pragma once
#ifndef _LAYER_H
#define _LAYER_H
#include "tensor.h"
#include "transfer_functions.h"

//General Layer for both host and device
template <template <class> class TT, class T>
class Layer
{
public:
	typedef TT<T> MatT;
	enum eLayerType
	{
		eLayerUnknown,
		eCLayer,
		ePLayer,
		eFLayer
	};

	Layer() : num_hessian_accums_(0)
	{
		//Derived_from<TT<T>, BTensor<T> >();
	};
	virtual ~Layer() {	};

	//Non-virtual functions
	void transferBackPropagate( );
	//Data access methods
	//Outputs access			
	virtual const MatT& out() const { return out_; };
	virtual const MatT& de_dx()const { return de_dx_; };
	virtual MatT& de_dx() { return de_dx_; };
	virtual const MatT& de_dw() const { return de_dw_; };
	virtual const MatT& de_db() const { return de_db_; };
	virtual const MatT& d2e_dx2()const { return d2e_dx2_; };
	virtual MatT& d2e_dx2() { return d2e_dx2_; };
	virtual const MatT& d2e_dw2() const { return d2e_dw2_; };
	virtual const MatT& d2e_db2() const { return d2e_db2_; };
	//Weights access
	virtual const MatT& weights() const {return weights_;};
	virtual const MatT& biases() const {return biases_;};
	//Setters
	virtual void set_de_dx(const MatT& dedx) { de_dx_ = dedx; };
	virtual void set_d2e_dx2(const MatT& dedx) { de_dx_ = dedx; };
	virtual void InitWeights( void(*weight_init_func)(TT<T>&))
	{
		//Apply to weights and biases
		weight_init_func(weights_);
		weight_init_func(biases_);
	}
	//Forward propagation	
	virtual void Propagate(const MatT& input)= 0;
	virtual void PrepareForTraining();
	/* Backpropagate error and compute gradients */
	virtual void BackPropagate(const MatT& input, MatT& dedx_prev)= 0;
	/* Backpropagate and accumulate Hessian. Before starting Hessian computation iterations,
	it should be reset. See Layer::Reset(). After iterations of Hessian 
	computation it should be averaged before use. See Layer::AverageHessian() */
	virtual void BackPropagateHessian(const MatT& input, MatT& d2edx2_prev) = 0;
	/* Compute gradient without backpropagating errors */
	virtual void ComputeGradient(const MatT& input) = 0;
	/* Compute Hessian without backpropagating errors */
	virtual void ComputeHessian(const MatT& input) = 0;
	/* Reset accumulated Hessian martix to 0*/
	virtual void ResetHessian() { d2e_dw2_.ZeroMemory(); d2e_db2_.ZeroMemory(); num_hessian_accums_ = 0; }
	/* Average the Hessian by the number of iterations it */
	// TODO: Generalize AverageHessian and AdaptWeights
	virtual void AverageHessian() = 0;
	virtual void AdaptWeights(T tau, bool use_hessian, T mu)= 0;
	// Loading and saving
	// Define interface for saving and loading
	class ILoadSaveObject
	{
	public:
		//Saving interface
		virtual void AddScalar(T scal, const std::string name) = 0; 
		virtual void AddScalar(int scal, const std::string name) = 0; 
		virtual void AddScalar(UINT scal, const std::string name) = 0; 
		virtual void AddArray(const TT<T>& arr, const std::string name) = 0; 
		virtual void AddArray(const TT<int>& arr, const std::string name) = 0; 
		virtual void AddString(const std::string str, const std::string name) = 0; 
		//Loading interface
		virtual void GetScalar(T& scal, const std::string name) = 0; 
		virtual void GetScalar(int& scal, const std::string name) = 0; 
		virtual void GetScalar(UINT& scal, const std::string name) = 0; 
		virtual void GetArray(TT<T>& arr, const std::string name) = 0; 
		virtual void GetArray(TT<int>& arr, const std::string name) = 0; 
		virtual void GetString(std::string& str, const std::string name) = 0; 
		virtual ~ILoadSaveObject(){};
	};
	virtual void Save(ILoadSaveObject& save_load_obj, bool dbg) = 0;
	virtual void Load(ILoadSaveObject& save_load_obj, bool dbg) = 0;
	virtual eLayerType layer_type() const = 0;

protected:
	eLayerType layer_type_; /*!< This layer type */
	//%----Output data
	MatT weighted_sum_; /*!< Weighted inputs before transfer function */
	MatT out_; /*!< Layer outputs */

	/* Weitgts */
	MatT weights_;
	/* Biases */
	MatT biases_;

	/* Error gradient (error derivative with respect to weights) */
	MatT de_dw_;
	/* Error gradient (error derivative with respect to biases) */
	MatT de_db_;
	/* Error derivative with respect to the output of the current layer.
	   Used as an output of backpropagation for next layer */
	MatT de_dx_;

	/* Second derrivatives */
	/* Error Hessian (error second derivative with respect to weights) */
	MatT d2e_dw2_;
	/* Error Hessian (error second derivative with respect to biases) */
	MatT d2e_db2_;
	/* Error second derivative with respect to current layer output.
	   Used as an output of backpropagation for next layer */
	MatT d2e_dx2_;
	/* Indicates that Hessian was accumulated but not averaged yet */
	//bool is_hessian_averaged;
	//Number of hessian accumulation calls. This is divider for hessian in averaging
	UINT num_hessian_accums_;
	float mu; 
};
template <template <class> class TT, class T>
void Layer<TT,T>::PrepareForTraining()
{
	if(!de_dw_.HaveSameSize(weights()))
		de_dw_ = Tensor<T>(weights().dims());
	if(!de_db_.HaveSameSize(biases()))
		de_db_ = Tensor<T>(biases().dims());
	if(!d2e_dw2_.HaveSameSize(weights()))
		d2e_dw2_ = Tensor<T>(weights().dims());
	if(!d2e_db2_.HaveSameSize(biases()))
		d2e_db2_ = Tensor<T>(biases().dims());
	if (!d2e_dx2_.HaveSameSize(out()))	
		d2e_dx2_ = out();
	if (!de_dx_.HaveSameSize(out()))	
		de_dx_ = out();
}

//===============================  Convolutional layer template
template <template <class> class TT, class T, class TF>
class CLayerT: public Layer<TT, T>
{
public:
	struct Params{
		UINT ninputs;
		UINT noutputs;
		UINT inp_width;
		UINT inp_height;
		UINT kernel_width;
		UINT kernel_height;
	};
	typedef TT<T> MainTensorType;
	virtual typename Layer<TT, T>::eLayerType layer_type() const { return eCLayer;};
	CLayerT(Params& params)
	{
		out_ = TT<T>(params.inp_width - params.kernel_width + 1, params.inp_height - params.kernel_height + 1,  params.noutputs);
		input_width_ = params.inp_width;
		input_height_ = params.inp_height;
		ninputs_ = params.ninputs;

		std::vector<UINT> wdims(4);
		wdims[0] = params.kernel_width;
		wdims[1] = params.kernel_height;
		wdims[2] = params.ninputs;
		wdims[3] = params.noutputs;
		weights_ = TT<T>(wdims);
		biases_ = TT<T>(1, 1, params.noutputs);
		std::vector<UINT> cdims(2);
		cdims[0] = params.ninputs;
		cdims[1] = params.noutputs;
		con_map_ = TT<int>(cdims);
	}
	CLayerT(UINT inp_width, UINT inp_height, 
		const MainTensorType& weights, const MainTensorType& biases, const TT<int>& con_map)
	{
		assert(weights.num_dims() == 4);
		out_ = MainTensorType(inp_width - weights.w()+ 1, inp_height - weights.h() + 1,  con_map.h());
		input_width_ = inp_width;
		input_height_ = inp_height;
		ninputs_ = con_map.w();

		weights_ = weights;
		biases_ = biases;
		con_map_ = con_map;
	};
	virtual void Save(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg = false);
	virtual void Load(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg = false);
	const TT<int>& con_map() const { return con_map_; };

protected:	
	TF transfer_function_;
	//Connection map
	TT<int> con_map_;
	UINT input_width_;
	UINT input_height_;
	UINT ninputs_;
};

template <template <class> class TT, class T, class TF> 
class CLayer;

template <template <class> class TT, class T, class TF>
void CLayerT<TT,T,TF>::Save(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg)
{
	save_load_obj.AddString("clayer", "LayerType");
	save_load_obj.AddScalar(weights().d2(),"NumFMaps");
	//save_load_obj.AddScalar(out().w(),"OutFMapWidth");
	//save_load_obj.AddScalar(out().h(),"OutFMapHeight");
    save_load_obj.AddScalar(input_width_,"InpWidth");
    save_load_obj.AddScalar(input_height_,"InpHeight");

	save_load_obj.AddArray(weights(),"Weights");
	save_load_obj.AddArray(biases(),"Biases");
	save_load_obj.AddArray(con_map(),"conn_map");
	save_load_obj.AddString(transfer_function_.name().c_str(),"TransferFunc");
	if(dbg){ //Add all data
		save_load_obj.AddArray(out(), "Output");
		save_load_obj.AddArray(de_dw_, "dEdW");
		save_load_obj.AddArray(de_db_, "dEdB");
		save_load_obj.AddArray(de_dx_, "dEdX_prev");
		save_load_obj.AddArray(d2e_dw2_, "d2EdW2");
		save_load_obj.AddArray(d2e_db2_, "d2EdB2");
		save_load_obj.AddArray(d2e_dx2_, "d2EdX2_prev");
	}
}

template <template <class> class TT, class T, class TF>
void CLayerT<TT,T,TF>::Load(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg)
{
	int num_fmaps, out_width, out_height;
	save_load_obj.GetScalar(num_fmaps,"NumFMaps");
	save_load_obj.GetScalar(out_width,"OutFMapWidth");
	save_load_obj.GetScalar(out_height,"OutFMapHeight");
	save_load_obj.GetArray(weights_,"Weights");
	save_load_obj.GetArray(biases_,"Biases");
	save_load_obj.GetArray(con_map_,"conn_map");
	input_width_ = out_width + weights_.w() - 1;
	input_height_ = out_height + weights_.h() - 1;
	ninputs_ = weights_.d();

	if(dbg){ //Add all data
		save_load_obj.GetArray(out_, "Output");
		save_load_obj.GetArray(de_dw_, "dEdW");
		save_load_obj.GetArray(de_db_, "dEdB");
		save_load_obj.GetArray(de_dx_, "dEdX_prev");
		save_load_obj.GetArray(d2e_dw2_, "d2EdW2");
		save_load_obj.GetArray(d2e_db2_, "d2EdB2");
		save_load_obj.GetArray(d2e_dx2_, "d2EdX2_prev");
	}
}

////===============================  Fully connected layer
template <template <class> class TT, class T, class TF>
class FLayerT: public Layer<TT, T>
{
public:
	typedef TT<T> MainTensorType;
	virtual typename Layer<TT, T>::eLayerType layer_type() const { return eFLayer;};

	FLayerT(const MainTensorType& weights, const MainTensorType& biases)
	{
		num_neurons_ = weights.w();
		out_ = MainTensorType(num_neurons_,1,1);   
		weights_ = weights;
		biases_ = biases;
	}
	
	virtual void Save(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg);
	virtual void Load(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg);
protected:
	TF transfer_function_;
	UINT num_neurons_;	
};

template <template <class> class TT, class T, class TF> 
class FLayer;


template <template <class> class TT, class T, class TF>
void FLayerT<TT, T, TF>::Save(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg)
{
	save_load_obj.AddString("flayer", "LayerType");
	save_load_obj.AddArray(weights(),"Weights");
	save_load_obj.AddArray(biases(),"Biases");
	save_load_obj.AddString(transfer_function_.name(), "TransferFunc");
	if(dbg){ //Add all data
		save_load_obj.AddArray(out(), "Output");
		save_load_obj.AddArray(de_dw_, "dEdW");
		save_load_obj.AddArray(de_db_, "dEdB");
		save_load_obj.AddArray(de_dx_, "dEdX_prev");
		save_load_obj.AddArray(d2e_dw2_, "d2EdW2");
		save_load_obj.AddArray(d2e_db2_, "d2EdB2");
		save_load_obj.AddArray(d2e_dx2_, "d2EdX2_prev");
	}
}

template <template <class> class TT, class T, class TF>
void FLayerT<TT, T, TF>::Load(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg)
{
	save_load_obj.GetArray(weights_,"Weights");
	save_load_obj.GetArray(biases_,"Biases");
	if(dbg){ //Add all data
		save_load_obj.GetArray(out_, "Output");
		save_load_obj.GetArray(de_dw_, "dEdW");
		save_load_obj.GetArray(de_db_, "dEdB");
		save_load_obj.GetArray(de_dx_, "dEdX_prev");
		save_load_obj.GetArray(d2e_dw2_, "d2EdW2");
		save_load_obj.GetArray(d2e_db2_, "d2EdB2");
		save_load_obj.GetArray(d2e_dx2_, "d2EdX2_prev");
	}
}
//Pooling layers

//Subsampling layer
template <template <class> class TT, class T> 
class PoolingLayerT: public Layer<TT, T >
{
public:
	typedef TT<T> MainTensorType;
    enum PoolingType{
        eUnknown,
        eAverage,
        eMax        
    } pooling_type_;
	virtual typename Layer<TT, T >::eLayerType layer_type() const { return ePLayer;};
	struct Params{
		UINT ninputs;
		UINT inp_width;
		UINT inp_height;
		UINT sx;    //Pooling window width
		UINT sy;    //Pooling window height
        PoolingType pooling_type;
	};

	PoolingLayerT(const Params& params)
	{
        pooling_type_ = params.pooling_type;
		sx_ = params.sx;
		sy_ = params.sy;
		input_width_ = params.inp_width;
		input_height_ = params.inp_height;
		ninputs_ = params.ninputs;
		//Init weights and biases as zero arrays
		std::vector<UINT> wdims(1);
		wdims[0] = 0;
		weights_ = TT<T>(wdims);
		biases_ = TT<T>(wdims);

		out_ = TT<T>(params.inp_width / params.sx, params.inp_height / params.sy, params.ninputs);
	}
	UINT sx() const { return sx_; }
	UINT sy() const { return sy_; }
    PoolingType pooling_type() const { return pooling_type_; }
	virtual void Save(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg);
	virtual void Load(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg);

protected:	
	UINT sx_;
	UINT sy_;
	UINT input_width_;
	UINT input_height_;
	UINT ninputs_;

};

template <template <class> class TT, class T> 
class PoolingLayer ;

//template <template <class> class TT, class T> 
//class MaxPoolingLayer;


template <template <class> class TT, class T>
void PoolingLayerT<TT, T>::Save(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg)
{
	save_load_obj.AddString("pooling", "LayerType");
	
	save_load_obj.AddScalar(out().d(),"NumFMaps");
	//save_load_obj.AddScalar(out().w(),"OutFMapWidth");
	//save_load_obj.AddScalar(out().h(),"OutFMapHeight");
    save_load_obj.AddScalar(input_width_,"InpWidth");
    save_load_obj.AddScalar(input_height_,"InpHeight");

	save_load_obj.AddScalar(sx(),"SXRate");
	save_load_obj.AddScalar(sy(),"SYRate");
    switch(pooling_type())
    {
    case eAverage: 
        save_load_obj.AddString("average", "PoolingType");
        break;
    case eMax: 
        save_load_obj.AddString("max", "PoolingType");
        break;
    default:
        throw std::runtime_error("Unknown pooling function detected while saving layer info");
    }
    //save_load_obj.AddScalar(static_cast<UINT>(pooling_type()),"PoolingType");
	if(dbg){ //Add all data
		save_load_obj.AddArray(out(), "Output");
		save_load_obj.AddArray(de_dw_, "dEdW");
		save_load_obj.AddArray(de_db_, "dEdB");
		save_load_obj.AddArray(de_dx_, "dEdX_prev");
		save_load_obj.AddArray(d2e_dw2_, "d2EdW2");
		save_load_obj.AddArray(d2e_db2_, "d2EdB2");
		save_load_obj.AddArray(d2e_dx2_, "d2EdX2_prev");
	}

}

template <template <class> class TT, class T>
void PoolingLayerT<TT, T>::Load(typename Layer<TT,T>::ILoadSaveObject& save_load_obj, bool dbg)
{
	int out_width, out_height;
	save_load_obj.GetScalar(ninputs_,"NumFMaps");
	save_load_obj.GetScalar(out_width,"OutFMapWidth");
	save_load_obj.GetScalar(out_height,"OutFMapHeight");

	save_load_obj.GetScalar(sx_,"SXRate");
	save_load_obj.GetScalar(sy_,"SYRate");

    std::string pooling_fnc;
    save_load_obj.GetString(pooling_fnc, "PoolingType");
    pooling_type_ = eUnknown;

    if(pooling_fnc.compare(std::string("average")) == 0) pooling_type_ = eAverage;
    if(pooling_fnc.compare(std::string("max")) == 0) pooling_type_ = eMax;
	input_width_ = out_width*sx_;
	input_height_ = out_height*sy_;

	if(dbg){ //Add all data
		save_load_obj.GetArray(out_, "Output");
		save_load_obj.GetArray(de_dw_, "dEdW");
		save_load_obj.GetArray(de_db_, "dEdB");
		save_load_obj.GetArray(de_dx_, "dEdX_prev");
		save_load_obj.GetArray(d2e_dw2_, "d2EdW2");
		save_load_obj.GetArray(d2e_db2_, "d2EdB2");
		save_load_obj.GetArray(d2e_dx2_, "d2EdX2_prev");
	}

}

#endif 

