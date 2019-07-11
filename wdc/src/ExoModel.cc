#include "exo_simu/core/Sensor.h"
#include "exo_simu/wdc/ExoModel.h"


namespace exo_simu
{
    ExoModel::ExoModel(void) :
    Model(),
    exoMdlOptions_(nullptr)
    {
        /* Model constructor calls the base implementations of the virtual methods since the derived 
           class is not available at this point. Thus it must be called explicitly in the constructor. */
        setOptions(getDefaultOptions());
    }

    ExoModel::~ExoModel(void)
    {
        // Empty.
    }

    result_t ExoModel::initialize(std::string const & urdfPath)
    {
        result_t returnCode = result_t::SUCCESS;

        // The contact frames are hard-coded so far
        contactFramesNames_ = std::vector<std::string>({std::string("LeftExternalToe"),
                                                        std::string("LeftInternalToe"),
                                                        std::string("LeftExternalHeel"),
                                                        std::string("LeftInternalHeel"),
                                                        std::string("RightInternalToe"),
                                                        std::string("RightExternalToe"),
                                                        std::string("RightInternalHeel"),
                                                        std::string("RightExternalHeel")});

        // Initialize base Model (cannot use custom bounds so far)
        configHolder_t & jointConfig = boost::get<configHolder_t>(mdlOptionsHolder_.at("joints"));
        configHolder_t jointConfigOld = jointConfig;
        boost::get<bool>(jointConfig.at("boundsFromUrdf")) = true;
        returnCode = Model::initialize(urdfPath, contactFramesNames_, jointsNames_); // The joint names are unknown at this point
        jointConfig = jointConfigOld;
        isInitialized_ = false; // ExoModel is not initialized so far

        std::vector<int32_t> imuFramesIdx;
        std::vector<std::string> imuFramesNames;
        if (returnCode == result_t::SUCCESS)
        {
            // The joint names are obtained by removing the universe and root joint from the joint list
            jointsNames_.assign(pncModel_.names.begin() + 2, pncModel_.names.end());

            // Discard the toes since they are not actuated nor physically meaningful
            jointsNames_.erase(
                std::remove_if(
                    jointsNames_.begin(), 
                    jointsNames_.end(),
                    [](std::string const & joint) -> bool
                    {
                        return joint.find("Toe") != std::string::npos;
                    }
                ), 
                jointsNames_.end()
            );

            /* Update the joint names manually to avoid calling back Model::initialize
               (It cannot throw an error since the names are extracted dynamically from the model) */
            getJointsIdx(jointsNames_, jointsPositionIdx_, jointsVelocityIdx_);

            // The names of the frames of the IMUs end with IMU
            for (int32_t i = 0; i < pncModel_.nframes; i++)
            {
                std::string frameNameCurrent = pncModel_.frames[i].name;
                if (frameNameCurrent.substr(frameNameCurrent.length() - 3) == "IMU")
                {
                    imuFramesNames.push_back(frameNameCurrent);
                }
            }
            getFramesIdx(imuFramesNames, imuFramesIdx); // It cannot throw an error
        }

        // ********** Add the IMU sensors **********
        for(uint32_t i = 0; i < imuFramesNames.size(); i++)
        {
            std::shared_ptr<ImuSensor> imuSensor;
            std::string imuName = ImuSensor::type_ + "." + imuFramesNames[i];

            if (returnCode == result_t::SUCCESS)
            {
                // Create a new IMU sensor
                returnCode = addSensor(imuName, imuSensor);
            }
            
            if (returnCode == result_t::SUCCESS)
            {
                // Configure the sensor
                imuSensor->initialize(imuFramesIdx[i]);
            }
        }
        
        // ********** Add the force sensors **********
        for (uint32_t i = 0; i<contactFramesNames_.size(); i++)
        {
            std::shared_ptr<ForceSensor> forceSensor;
            std::string forceName = ForceSensor::type_ + "." + contactFramesNames_[i];

            if (returnCode == result_t::SUCCESS)
            {
                // Create a new force sensor
                returnCode = addSensor(forceName, forceSensor);
            }

            if (returnCode == result_t::SUCCESS)
            {
                // Configure the sensor
                forceSensor->initialize(contactFramesIdx_[i]);
            }
        }

        // ********** Add the encoder sensors **********
        for (uint32_t i = 0; i<jointsNames_.size(); i++)
        {
            std::shared_ptr<EncoderSensor> encoderSensor;
            std::string encoderName = EncoderSensor::type_ + "." + jointsNames_[i];

            if (returnCode == result_t::SUCCESS)
            {
                // Create a new encoder sensor
                returnCode = addSensor(encoderName, encoderSensor);
            }
            
            if (returnCode == result_t::SUCCESS)
            {
                // Configure the sensor
                encoderSensor->initialize(jointsPositionIdx_[i], jointsVelocityIdx_[i]);
            }
        }

         // Update the bounds if necessary and set the initialization flag
        if (returnCode == result_t::SUCCESS)
        {
            isInitialized_ = true;
            returnCode = setOptions(mdlOptionsHolder_);
        }

        return returnCode;
    }

    result_t ExoModel::configureTelemetry(std::shared_ptr<TelemetryData> const & telemetryData)
    {
        result_t returnCode = result_t::SUCCESS;

        returnCode = Model::configureTelemetry(telemetryData);

        for (sensorsGroupHolder_t::value_type const & sensorGroup : sensorsGroupHolder_)
        {
            for (sensorsHolder_t::value_type const & sensor : sensorGroup.second)
            {
                if (returnCode == result_t::SUCCESS)
                {
                    if (sensorGroup.first == ImuSensor::type_)
                    {
                        if (exoMdlOptions_->telemetry.logImuSensors)
                        {
                            returnCode = sensor.second->configureTelemetry(telemetryData_);
                        }
                    }
                    else if (sensorGroup.first == ForceSensor::type_)
                    {
                        if (exoMdlOptions_->telemetry.logForceSensors)
                        {
                            returnCode = sensor.second->configureTelemetry(telemetryData_);
                        }
                    }
                    else
                    {
                        if (exoMdlOptions_->telemetry.logEncoderSensors)
                        {
                            returnCode = sensor.second->configureTelemetry(telemetryData_);
                        }
                    }
                }
            }
        }

        return returnCode;
    }

    configHolder_t ExoModel::getSensorsOptions(void) const
    {
        configHolder_t sensorOptions;

        sensorOptions[ImuSensor::type_] = ImuSensor::getOptions();
        sensorOptions[ForceSensor::type_] = ForceSensor::getOptions();
        sensorOptions[EncoderSensor::type_] = EncoderSensor::getOptions();

        return sensorOptions;
    }
    
    void ExoModel::setSensorsOptions(configHolder_t & sensorOptions)
    {
        // Set sensor options and delete the associated key, if any

        configHolder_t::const_iterator it = sensorOptions.find(ImuSensor::type_);
        if (it != sensorOptions.end())
        {
            ImuSensor::setOptions(boost::get<configHolder_t>(sensorOptions.at(ImuSensor::type_)));
            sensorOptions.erase(ImuSensor::type_);
        }
        it = sensorOptions.find(ForceSensor::type_);
        if (it != sensorOptions.end())
        {
            ForceSensor::setOptions(boost::get<configHolder_t>(sensorOptions.at(ForceSensor::type_)));
            sensorOptions.erase(ForceSensor::type_);
        }
        it = sensorOptions.find(EncoderSensor::type_);
        if (it != sensorOptions.end())
        {
            EncoderSensor::setOptions(boost::get<configHolder_t>(sensorOptions.at(EncoderSensor::type_)));
            sensorOptions.erase(EncoderSensor::type_);
        }
    }

    result_t ExoModel::setOptions(configHolder_t const & mdlOptions)
    {
        result_t returnCode = result_t::SUCCESS;

        returnCode = Model::setOptions(mdlOptions);
        if (returnCode == result_t::SUCCESS)
        {
            exoMdlOptions_ = std::make_unique<exoModelOptions_t const>(mdlOptionsHolder_);
        }

        return returnCode;
    }
}